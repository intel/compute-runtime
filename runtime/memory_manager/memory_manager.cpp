/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/memory_manager.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/event/event.h"
#include "runtime/event/hw_timestamps.h"
#include "runtime/event/perf_counter.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/deferrable_allocation_deletion.h"
#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/memory_manager/host_ptr_manager.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/mem_obj/image.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/utilities/stackvec.h"

#include <algorithm>

namespace OCLRT {
MemoryManager::MemoryManager(bool enable64kbpages, bool enableLocalMemory,
                             ExecutionEnvironment &executionEnvironment) : allocator32Bit(nullptr), enable64kbpages(enable64kbpages),
                                                                           localMemorySupported(enableLocalMemory),
                                                                           executionEnvironment(executionEnvironment),
                                                                           hostPtrManager(std::make_unique<HostPtrManager>()),
                                                                           multiContextResourceDestructor(std::make_unique<DeferredDeleter>()) {
    registeredOsContexts.resize(1);
};

MemoryManager::~MemoryManager() {
    for (auto osContext : registeredOsContexts) {
        if (osContext) {
            osContext->decRefInternal();
        }
    }
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

GraphicsAllocation *MemoryManager::allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) {
    if (deferredDeleter) {
        deferredDeleter->drain(true);
    }
    GraphicsAllocation *graphicsAllocation = nullptr;
    auto osStorage = hostPtrManager->prepareOsStorageForAllocation(*this, allocationData.size, allocationData.hostPtr);
    if (osStorage.fragmentCount > 0) {
        graphicsAllocation = createGraphicsAllocation(osStorage, allocationData.size, allocationData.hostPtr);
    }
    return graphicsAllocation;
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemoryForImageFromHostPtr(ImageInfo &imgInfo, const void *hostPtr) {
    bool copyRequired = Image::isCopyRequired(imgInfo, hostPtr);

    if (hostPtr && !copyRequired) {
        return allocateGraphicsMemory({false, imgInfo.size, GraphicsAllocation::AllocationType::UNDECIDED}, hostPtr);
    }
    return nullptr;
}

void MemoryManager::cleanGraphicsMemoryCreatedFromHostPtr(GraphicsAllocation *graphicsAllocation) {
    hostPtrManager->releaseHandleStorage(graphicsAllocation->fragmentsStorage);
    cleanOsHandles(graphicsAllocation->fragmentsStorage);
}

GraphicsAllocation *MemoryManager::createGraphicsAllocationWithPadding(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) {
    if (!paddingAllocation) {
        paddingAllocation = allocateGraphicsMemoryWithProperties({paddingBufferSize, GraphicsAllocation::AllocationType::UNDECIDED});
    }
    return createPaddedAllocation(inputGraphicsAllocation, sizeWithPadding);
}

GraphicsAllocation *MemoryManager::createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) {
    return allocateGraphicsMemoryWithProperties({sizeWithPadding, GraphicsAllocation::AllocationType::UNDECIDED});
}

void MemoryManager::freeSystemMemory(void *ptr) {
    ::alignedFree(ptr);
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
}

void MemoryManager::freeGraphicsMemory(GraphicsAllocation *gfxAllocation) {
    freeGraphicsMemoryImpl(gfxAllocation);
}
//if not in use destroy in place
//if in use pass to temporary allocation list that is cleaned on blocking calls
void MemoryManager::checkGpuUsageAndDestroyGraphicsAllocations(GraphicsAllocation *gfxAllocation) {
    if (gfxAllocation->isUsed()) {
        if (gfxAllocation->isUsedByManyOsContexts()) {
            multiContextResourceDestructor->deferDeletion(new DeferrableAllocationDeletion{*this, *gfxAllocation});
            multiContextResourceDestructor->drain(false);
            return;
        }
        for (auto &deviceCsrs : getCommandStreamReceivers()) {
            for (auto &csr : deviceCsrs) {
                if (csr) {
                    auto osContextId = csr->getOsContext().getContextId();
                    auto allocationTaskCount = gfxAllocation->getTaskCount(osContextId);
                    if (gfxAllocation->isUsedByOsContext(osContextId) &&
                        allocationTaskCount > *csr->getTagAddress()) {
                        csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(gfxAllocation), TEMPORARY_ALLOCATION);
                        return;
                    }
                }
            }
        }
    }
    freeGraphicsMemory(gfxAllocation);
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

bool MemoryManager::isLocalMemorySupported() const {
    return localMemorySupported;
}

bool MemoryManager::isMemoryBudgetExhausted() const {
    return false;
}

OsContext *MemoryManager::createAndRegisterOsContext(EngineInstanceT engineType, PreemptionMode preemptionMode) {
    auto contextId = ++latestContextId;
    if (contextId + 1 > registeredOsContexts.size()) {
        registeredOsContexts.resize(contextId + 1);
    }
    auto osContext = new OsContext(executionEnvironment.osInterface.get(), contextId, engineType, preemptionMode);
    osContext->incRefInternal();
    registeredOsContexts[contextId] = osContext;

    return osContext;
}

bool MemoryManager::getAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const DevicesBitfield devicesBitfield,
                                      const void *hostPtr) {
    UNRECOVERABLE_IF(hostPtr == nullptr && !properties.flags.allocateMemory);

    bool allow64KbPages = false;
    bool allow32Bit = false;
    bool forcePin = properties.flags.forcePin;
    bool uncacheable = properties.flags.uncacheable;
    bool mustBeZeroCopy = false;
    bool multiOsContextCapable = properties.flags.multiOsContextCapable;

    switch (properties.allocationType) {
    case GraphicsAllocation::AllocationType::BUFFER:
    case GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY:
    case GraphicsAllocation::AllocationType::BUFFER_COMPRESSED:
    case GraphicsAllocation::AllocationType::PIPE:
    case GraphicsAllocation::AllocationType::SCRATCH_SURFACE:
    case GraphicsAllocation::AllocationType::PRIVATE_SURFACE:
    case GraphicsAllocation::AllocationType::PRINTF_SURFACE:
    case GraphicsAllocation::AllocationType::CONSTANT_SURFACE:
    case GraphicsAllocation::AllocationType::GLOBAL_SURFACE:
        allow64KbPages = true;
        allow32Bit = true;
        break;
    default:
        break;
    }

    switch (properties.allocationType) {
    case GraphicsAllocation::AllocationType::SVM:
        allow64KbPages = true;
        break;
    default:
        break;
    }

    switch (properties.allocationType) {
    case GraphicsAllocation::AllocationType::BUFFER:
    case GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY:
    case GraphicsAllocation::AllocationType::BUFFER_COMPRESSED:
        forcePin = true;
        break;
    default:
        break;
    }

    switch (properties.allocationType) {
    case GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY:
    case GraphicsAllocation::AllocationType::PIPE:
    case GraphicsAllocation::AllocationType::PRINTF_SURFACE:
    case GraphicsAllocation::AllocationType::CONSTANT_SURFACE:
    case GraphicsAllocation::AllocationType::GLOBAL_SURFACE:
    case GraphicsAllocation::AllocationType::SVM:
        mustBeZeroCopy = true;
        break;
    default:
        break;
    }

    switch (properties.allocationType) {
    case GraphicsAllocation::AllocationType::UNDECIDED:
    case GraphicsAllocation::AllocationType::LINEAR_STREAM:
    case GraphicsAllocation::AllocationType::FILL_PATTERN:
    case GraphicsAllocation::AllocationType::TIMESTAMP_TAG_BUFFER:
        allocationData.flags.useSystemMemory = true;
        break;
    default:
        break;
    }

    allocationData.flags.mustBeZeroCopy = mustBeZeroCopy;
    allocationData.flags.allocateMemory = properties.flags.allocateMemory;
    allocationData.flags.allow32Bit = allow32Bit;
    allocationData.flags.allow64kbPages = allow64KbPages;
    allocationData.flags.forcePin = forcePin;
    allocationData.flags.uncacheable = uncacheable;
    allocationData.flags.flushL3 = properties.flags.flushL3RequiredForRead | properties.flags.flushL3RequiredForWrite;
    allocationData.flags.preferRenderCompressed = GraphicsAllocation::AllocationType::BUFFER_COMPRESSED == properties.allocationType;
    allocationData.flags.multiOsContextCapable = multiOsContextCapable;

    if (allocationData.flags.mustBeZeroCopy) {
        allocationData.flags.useSystemMemory = true;
    }

    allocationData.hostPtr = hostPtr;
    allocationData.size = properties.size;
    allocationData.type = properties.allocationType;
    allocationData.devicesBitfield = devicesBitfield;
    allocationData.alignment = properties.alignment ? properties.alignment : MemoryConstants::preferredAlignment;
    allocationData.imgInfo = properties.imgInfo;

    if (allocationData.flags.allocateMemory) {
        allocationData.hostPtr = nullptr;
    }
    return true;
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemoryInPreferredPool(AllocationProperties properties, DevicesBitfield devicesBitfield, const void *hostPtr) {
    AllocationData allocationData;
    AllocationStatus status = AllocationStatus::Error;

    getAllocationData(allocationData, properties, devicesBitfield, hostPtr);
    UNRECOVERABLE_IF(allocationData.type == GraphicsAllocation::AllocationType::SHARED_RESOURCE);
    GraphicsAllocation *allocation = nullptr;

    allocation = allocateGraphicsMemoryInDevicePool(allocationData, status);
    if (!allocation && status == AllocationStatus::RetryInNonDevicePool) {
        allocation = allocateGraphicsMemory(allocationData);
    }
    if (allocation) {
        allocation->setAllocationType(properties.allocationType);
    }

    return allocation;
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemory(const AllocationData &allocationData) {
    if (allocationData.type == GraphicsAllocation::AllocationType::IMAGE) {
        UNRECOVERABLE_IF(allocationData.imgInfo == nullptr);
        return allocateGraphicsMemoryForImage(*allocationData.imgInfo, allocationData.hostPtr);
    }

    if (force32bitAllocations && allocationData.flags.allow32Bit && is64bit) {
        return allocate32BitGraphicsMemory(allocationData.size, allocationData.hostPtr, AllocationOrigin::EXTERNAL_ALLOCATION);
    }
    if (allocationData.hostPtr) {
        return allocateGraphicsMemoryWithHostPtr(allocationData);
    }
    if (peek64kbPagesEnabled() && allocationData.flags.allow64kbPages) {
        return allocateGraphicsMemory64kb(allocationData);
    }
    return allocateGraphicsMemoryWithAlignment(allocationData);
}

const CsrContainer &MemoryManager::getCommandStreamReceivers() const {
    return executionEnvironment.commandStreamReceivers;
}

CommandStreamReceiver *MemoryManager::getDefaultCommandStreamReceiver(uint32_t deviceId) const {
    return executionEnvironment.commandStreamReceivers[deviceId][defaultEngineIndex].get();
}

} // namespace OCLRT
