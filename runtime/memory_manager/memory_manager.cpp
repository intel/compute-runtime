/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/event/event.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/options.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/utilities/stackvec.h"
#include "runtime/utilities/tag_allocator.h"
#include "runtime/event/hw_timestamps.h"
#include "runtime/event/perf_counter.h"

#include <algorithm>

namespace OCLRT {
constexpr size_t ProfilingTagCount = 512;
constexpr size_t PerfCounterTagCount = 512;

struct ReusableAllocationRequirements {
    size_t requiredMinimalSize;
    volatile uint32_t *csrTagAddress;
    bool internalAllocationRequired;
};

std::unique_ptr<GraphicsAllocation> AllocationsList::detachAllocation(size_t requiredMinimalSize, volatile uint32_t *csrTagAddress, bool internalAllocationRequired) {
    ReusableAllocationRequirements req;
    req.requiredMinimalSize = requiredMinimalSize;
    req.csrTagAddress = csrTagAddress;
    req.internalAllocationRequired = internalAllocationRequired;
    GraphicsAllocation *a = nullptr;
    GraphicsAllocation *retAlloc = processLocked<AllocationsList, &AllocationsList::detachAllocationImpl>(a, static_cast<void *>(&req));
    return std::unique_ptr<GraphicsAllocation>(retAlloc);
}

GraphicsAllocation *AllocationsList::detachAllocationImpl(GraphicsAllocation *, void *data) {
    ReusableAllocationRequirements *req = static_cast<ReusableAllocationRequirements *>(data);
    auto *curr = head;
    while (curr != nullptr) {
        auto currentTagValue = req->csrTagAddress ? *req->csrTagAddress : -1;
        if ((req->internalAllocationRequired == curr->is32BitAllocation) &&
            (curr->getUnderlyingBufferSize() >= req->requiredMinimalSize) &&
            ((currentTagValue > curr->taskCount) || (curr->taskCount == 0))) {
            return removeOneImpl(curr, nullptr);
        }
        curr = curr->next;
    }
    return nullptr;
}
MemoryManager::MemoryManager(bool enable64kbpages) : allocator32Bit(nullptr), enable64kbpages(enable64kbpages) {
    residencyAllocations.reserve(20);
};
MemoryManager::~MemoryManager() {
    freeAllocationsList(-1, graphicsAllocations);
    freeAllocationsList(-1, allocationsForReuse);
}

void *MemoryManager::allocateSystemMemory(size_t size, size_t alignment) {
    // Establish a minimum alignment of 16bytes.
    constexpr size_t minAlignment = 16;
    alignment = std::max(alignment, minAlignment);
    auto restrictions = getAlignedMallocRestrictions();
    void *ptr = nullptr;

    ptr = alignedMallocWrapper(size, alignment);
    if (restrictions == nullptr) {
        return ptr;
    } else if (restrictions->minAddress == 0) {
        return ptr;
    } else {
        if (restrictions->minAddress > reinterpret_cast<uintptr_t>(ptr) && ptr != nullptr) {
            StackVec<void *, 100> invalidMemVector;
            invalidMemVector.push_back(ptr);
            do {
                ptr = alignedMallocWrapper(size, alignment);
                if (restrictions->minAddress > reinterpret_cast<uintptr_t>(ptr) && ptr != nullptr) {
                    invalidMemVector.push_back(ptr);
                } else {
                    break;
                }
            } while (1);
            for (auto &it : invalidMemVector) {
                alignedFreeWrapper(it);
            }
        }
    }

    return ptr;
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemoryForSVM(size_t size, bool coherent) {
    GraphicsAllocation *graphicsAllocation = nullptr;
    if (enable64kbpages) {
        graphicsAllocation = allocateGraphicsMemory64kb(size, MemoryConstants::pageSize64k, false);
    } else {
        graphicsAllocation = allocateGraphicsMemory(size, MemoryConstants::pageSize);
    }
    if (graphicsAllocation) {
        graphicsAllocation->setCoherent(coherent);
    }
    return graphicsAllocation;
}

void MemoryManager::freeGmm(GraphicsAllocation *gfxAllocation) {
    delete gfxAllocation->gmm;
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemory(size_t size, const void *ptr, bool forcePin) {
    std::lock_guard<decltype(mtx)> lock(mtx);
    auto requirements = HostPtrManager::getAllocationRequirements(ptr, size);
    GraphicsAllocation *graphicsAllocation = nullptr;

    if (deferredDeleter) {
        deferredDeleter->drain(true);
    }

    //check for overlaping
    CheckedFragments checkedFragments;
    if (checkAllocationsForOverlapping(&requirements, &checkedFragments) == RequirementsStatus::FATAL) {
        //abort whole application instead of silently passing.
        abortExecution();
    }

    auto osStorage = hostPtrManager.populateAlreadyAllocatedFragments(requirements, &checkedFragments);
    if (osStorage.fragmentCount == 0) {
        return nullptr;
    }
    auto result = populateOsHandles(osStorage);
    if (result != AllocationStatus::Success) {
        cleanOsHandles(osStorage);
        return nullptr;
    }

    graphicsAllocation = createGraphicsAllocation(osStorage, size, ptr);
    return graphicsAllocation;
}

void MemoryManager::cleanGraphicsMemoryCreatedFromHostPtr(GraphicsAllocation *graphicsAllocation) {
    hostPtrManager.releaseHandleStorage(graphicsAllocation->fragmentsStorage);
    cleanOsHandles(graphicsAllocation->fragmentsStorage);
}

GraphicsAllocation *MemoryManager::createGraphicsAllocationWithPadding(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) {
    if (!paddingAllocation) {
        paddingAllocation = allocateGraphicsMemory(paddingBufferSize, MemoryConstants::pageSize);
    }
    return createPaddedAllocation(inputGraphicsAllocation, sizeWithPadding);
}

GraphicsAllocation *MemoryManager::createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) {
    return allocateGraphicsMemory(sizeWithPadding);
}

void MemoryManager::freeSystemMemory(void *ptr) {
    ::alignedFree(ptr);
}

void MemoryManager::storeAllocation(std::unique_ptr<GraphicsAllocation> gfxAllocation, uint32_t allocationUsage) {
    std::lock_guard<decltype(mtx)> lock(mtx);

    uint32_t taskCount = gfxAllocation->taskCount;

    if (allocationUsage == REUSABLE_ALLOCATION) {
        if (csr) {
            taskCount = csr->peekTaskCount();
        } else {
            taskCount = 0;
        }
    }

    storeAllocation(std::move(gfxAllocation), allocationUsage, taskCount);
}

void MemoryManager::storeAllocation(std::unique_ptr<GraphicsAllocation> gfxAllocation, uint32_t allocationUsage, uint32_t taskCount) {
    std::lock_guard<decltype(mtx)> lock(mtx);

    if (DebugManager.flags.DisableResourceRecycling.get()) {
        if (allocationUsage == REUSABLE_ALLOCATION) {
            freeGraphicsMemory(gfxAllocation.release());
            return;
        }
    }

    auto &allocationsList = (allocationUsage == TEMPORARY_ALLOCATION) ? graphicsAllocations : allocationsForReuse;
    gfxAllocation->taskCount = taskCount;
    allocationsList.pushTailOne(*gfxAllocation.release());
}

std::unique_ptr<GraphicsAllocation> MemoryManager::obtainReusableAllocation(size_t requiredSize, bool internalAllocation) {
    std::lock_guard<decltype(mtx)> lock(mtx);
    auto allocation = allocationsForReuse.detachAllocation(requiredSize, csr ? csr->getTagAddress() : nullptr, internalAllocation);
    return allocation;
}

void MemoryManager::setForce32BitAllocations(bool newValue) {
    if (newValue && !this->allocator32Bit) {
        this->allocator32Bit.reset(new Allocator32bit);
    }
    force32bitAllocations = newValue;
}

void MemoryManager::applyCommonCleanup() {
    if (this->paddingAllocation) {
        this->freeGraphicsMemory(this->paddingAllocation);
    }
    if (profilingTimeStampAllocator)
        profilingTimeStampAllocator->cleanUpResources();

    if (perfCounterAllocator)
        perfCounterAllocator->cleanUpResources();

    cleanAllocationList(-1, TEMPORARY_ALLOCATION);
    cleanAllocationList(-1, REUSABLE_ALLOCATION);
}

bool MemoryManager::cleanAllocationList(uint32_t waitTaskCount, uint32_t allocationUsage) {
    std::lock_guard<decltype(mtx)> lock(mtx);
    freeAllocationsList(waitTaskCount, (allocationUsage == TEMPORARY_ALLOCATION) ? graphicsAllocations : allocationsForReuse);
    return false;
}

void MemoryManager::freeAllocationsList(uint32_t waitTaskCount, AllocationsList &allocationsList) {
    GraphicsAllocation *curr = allocationsList.detachNodes();

    IDList<GraphicsAllocation, false, true> allocationsLeft;
    while (curr != nullptr) {
        auto *next = curr->next;
        if (curr->taskCount <= waitTaskCount) {
            freeGraphicsMemory(curr);
        } else {
            allocationsLeft.pushTailOne(*curr);
        }
        curr = next;
    }

    if (allocationsLeft.peekIsEmpty() == false) {
        allocationsList.splice(*allocationsLeft.detachNodes());
    }
}

TagAllocator<HwTimeStamps> *MemoryManager::getEventTsAllocator() {
    if (profilingTimeStampAllocator.get() == nullptr) {
        profilingTimeStampAllocator.reset(new TagAllocator<HwTimeStamps>(this, ProfilingTagCount, MemoryConstants::cacheLineSize));
    }
    return profilingTimeStampAllocator.get();
}

TagAllocator<HwPerfCounter> *MemoryManager::getEventPerfCountAllocator() {
    if (perfCounterAllocator.get() == nullptr) {
        perfCounterAllocator.reset(new TagAllocator<HwPerfCounter>(this, PerfCounterTagCount, MemoryConstants::cacheLineSize));
    }
    return perfCounterAllocator.get();
}

void MemoryManager::pushAllocationForResidency(GraphicsAllocation *gfxAllocation) {
    residencyAllocations.push_back(gfxAllocation);
}

void MemoryManager::clearResidencyAllocations() {
    residencyAllocations.clear();
}

void MemoryManager::pushAllocationForEviction(GraphicsAllocation *gfxAllocation) {
    evictionAllocations.push_back(gfxAllocation);
}

void MemoryManager::clearEvictionAllocations() {
    evictionAllocations.clear();
}

void MemoryManager::freeGraphicsMemory(GraphicsAllocation *gfxAllocation) {
    freeGraphicsMemoryImpl(gfxAllocation);
}
//if not in use destroy in place
//if in use pass to temporary allocation list that is cleaned on blocking calls
void MemoryManager::checkGpuUsageAndDestroyGraphicsAllocations(GraphicsAllocation *gfxAllocation) {
    if (gfxAllocation->taskCount == ObjectNotUsed || gfxAllocation->taskCount <= *csr->getTagAddress()) {
        freeGraphicsMemory(gfxAllocation);
    } else {
        storeAllocation(std::unique_ptr<GraphicsAllocation>(gfxAllocation), TEMPORARY_ALLOCATION);
    }
}

void MemoryManager::waitForDeletions() {
    if (deferredDeleter) {
        deferredDeleter->drain(false);
    }
    deferredDeleter.reset(nullptr);
}
bool MemoryManager::isAsyncDeleterEnabled() const {
    return asyncDeleterEnabled;
}

bool MemoryManager::isMemoryBudgetExhausted() const {
    return false;
}

RequirementsStatus MemoryManager::checkAllocationsForOverlapping(AllocationRequirements *requirements, CheckedFragments *checkedFragments) {
    DEBUG_BREAK_IF(requirements == nullptr);
    DEBUG_BREAK_IF(checkedFragments == nullptr);

    RequirementsStatus status = RequirementsStatus::SUCCESS;
    checkedFragments->count = 0;

    for (unsigned int i = 0; i < max_fragments_count; i++) {
        checkedFragments->status[i] = OverlapStatus::FRAGMENT_NOT_CHECKED;
        checkedFragments->fragments[i] = nullptr;
    }

    for (unsigned int i = 0; i < requirements->requiredFragmentsCount; i++) {
        checkedFragments->count++;
        checkedFragments->fragments[i] = hostPtrManager.getFragmentAndCheckForOverlaps(requirements->AllocationFragments[i].allocationPtr, requirements->AllocationFragments[i].allocationSize, checkedFragments->status[i]);
        if (checkedFragments->status[i] == OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT) {
            // clean temporary allocations
            if (csr != nullptr) {
                uint32_t taskCount = *csr->getTagAddress();
                cleanAllocationList(taskCount, TEMPORARY_ALLOCATION);

                // check overlapping again
                checkedFragments->fragments[i] = hostPtrManager.getFragmentAndCheckForOverlaps(requirements->AllocationFragments[i].allocationPtr, requirements->AllocationFragments[i].allocationSize, checkedFragments->status[i]);

                if (checkedFragments->status[i] == OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT) {
                    // Wait for completion
                    while (*csr->getTagAddress() < csr->peekLatestSentTaskCount()) {
                    }

                    taskCount = *csr->getTagAddress();
                    cleanAllocationList(taskCount, TEMPORARY_ALLOCATION);

                    // check overlapping last time
                    checkedFragments->fragments[i] = hostPtrManager.getFragmentAndCheckForOverlaps(requirements->AllocationFragments[i].allocationPtr, requirements->AllocationFragments[i].allocationSize, checkedFragments->status[i]);
                    if (checkedFragments->status[i] == OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT) {
                        status = RequirementsStatus::FATAL;
                        break;
                    }
                }
            } else {
                // This path is tested in ULTs
                status = RequirementsStatus::FATAL;
                break;
            }
        }
    }
    return status;
}

} // namespace OCLRT
