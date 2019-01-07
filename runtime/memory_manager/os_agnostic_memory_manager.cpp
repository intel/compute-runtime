/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/memory_manager/host_ptr_manager.h"
#include <cassert>

namespace OCLRT {

OsAgnosticMemoryManager::~OsAgnosticMemoryManager() {
    if (DebugManager.flags.UseMallocToObtainHeap32Base.get()) {
        alignedFreeWrapper(reinterpret_cast<void *>(allocator32Bit->getBase()));
    }
    applyCommonCleanup();
}

struct OsHandle {
};

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {

    auto sizeAligned = alignUp(allocationData.size, MemoryConstants::pageSize);
    MemoryAllocation *memoryAllocation = nullptr;

    if (fakeBigAllocations && allocationData.size > bigAllocation) {
        memoryAllocation = new MemoryAllocation(nullptr, (void *)dummyAddress, static_cast<uint64_t>(dummyAddress), allocationData.size, counter,
                                                MemoryPool::System4KBPages, this->getOsContextCount(), allocationData.flags.multiOsContextCapable);
        counter++;
        memoryAllocation->uncacheable = allocationData.flags.uncacheable;
        return memoryAllocation;
    }
    auto ptr = allocateSystemMemory(sizeAligned, allocationData.alignment ? alignUp(allocationData.alignment, MemoryConstants::pageSize) : MemoryConstants::pageSize);
    if (ptr != nullptr) {
        memoryAllocation = new MemoryAllocation(ptr, ptr, reinterpret_cast<uint64_t>(ptr), allocationData.size, counter, MemoryPool::System4KBPages,
                                                this->getOsContextCount(), allocationData.flags.multiOsContextCapable);
        if (!memoryAllocation) {
            alignedFreeWrapper(ptr);
            return nullptr;
        }
        memoryAllocation->uncacheable = allocationData.flags.uncacheable;
    }
    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(size_t size, void *cpuPtr) {
    MemoryAllocation *memoryAllocation = nullptr;
    auto alignedPtr = alignDown(reinterpret_cast<char *>(cpuPtr), MemoryConstants::pageSize);
    auto offsetInPage = reinterpret_cast<char *>(cpuPtr) - alignedPtr;

    memoryAllocation = new MemoryAllocation(nullptr, cpuPtr, reinterpret_cast<uint64_t>(alignedPtr), size, counter, MemoryPool::System4KBPages,
                                            this->getOsContextCount(), false);

    memoryAllocation->allocationOffset = offsetInPage;
    memoryAllocation->uncacheable = false;

    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemory64kb(AllocationData allocationData) {
    allocationData.size = alignUp(allocationData.size, MemoryConstants::pageSize64k);
    allocationData.alignment = MemoryConstants::pageSize64k;
    auto memoryAllocation = allocateGraphicsMemoryWithAlignment(allocationData);
    if (memoryAllocation) {
        reinterpret_cast<MemoryAllocation *>(memoryAllocation)->overrideMemoryPool(MemoryPool::System64KBPages);
    }
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocate32BitGraphicsMemory(size_t size, const void *ptr, AllocationOrigin allocationOrigin) {
    if (ptr) {
        auto allocationSize = alignSizeWholePage(ptr, size);
        auto gpuVirtualAddress = allocator32Bit->allocate(allocationSize);
        if (!gpuVirtualAddress) {
            return nullptr;
        }
        uint64_t offset = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr) & MemoryConstants::pageMask);
        MemoryAllocation *memAlloc = new MemoryAllocation(nullptr, const_cast<void *>(ptr), GmmHelper::canonize(gpuVirtualAddress + offset), size,
                                                          counter, MemoryPool::System4KBPagesWith32BitGpuAddressing, this->getOsContextCount(), false);
        memAlloc->is32BitAllocation = true;
        memAlloc->gpuBaseAddress = GmmHelper::canonize(allocator32Bit->getBase());
        memAlloc->sizeToFree = allocationSize;

        counter++;
        return memAlloc;
    }

    auto allocationSize = alignUp(size, MemoryConstants::pageSize);
    void *ptrAlloc = nullptr;
    auto gpuAddress = allocator32Bit->allocate(allocationSize);

    auto freeCpuPointer = true;

    if (DebugManager.flags.UseMallocToObtainHeap32Base.get()) {
        ptrAlloc = reinterpret_cast<void *>(gpuAddress);
        freeCpuPointer = false;
    } else {
        if (size < 0xfffff000) {
            ptrAlloc = alignedMallocWrapper(allocationSize, MemoryConstants::allocationAlignment);
        }
    }

    MemoryAllocation *memoryAllocation = nullptr;
    if (ptrAlloc != nullptr) {
        memoryAllocation = new MemoryAllocation(freeCpuPointer ? ptrAlloc : nullptr, ptrAlloc, GmmHelper::canonize(gpuAddress), size, counter,
                                                MemoryPool::System4KBPagesWith32BitGpuAddressing, this->getOsContextCount(), false);
        memoryAllocation->is32BitAllocation = true;
        memoryAllocation->gpuBaseAddress = GmmHelper::canonize(allocator32Bit->getBase());
        memoryAllocation->sizeToFree = allocationSize;
    }
    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) {
    auto graphicsAllocation = new MemoryAllocation(nullptr, reinterpret_cast<void *>(1), 1, 4096u, static_cast<uint64_t>(handle),
                                                   MemoryPool::SystemCpuInaccessible, this->getOsContextCount(), false);
    graphicsAllocation->setSharedHandle(handle);
    graphicsAllocation->is32BitAllocation = requireSpecificBitness;
    return graphicsAllocation;
}

void OsAgnosticMemoryManager::addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) {
    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = gfxAllocation->getUnderlyingBuffer();
    fragment.fragmentSize = alignUp(gfxAllocation->getUnderlyingBufferSize(), MemoryConstants::pageSize);
    fragment.osInternalStorage = new OsHandle();
    fragment.residency = new ResidencyData();
    hostPtrManager->storeFragment(fragment);
}

void OsAgnosticMemoryManager::removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) {
    auto buffer = gfxAllocation->getUnderlyingBuffer();
    auto fragment = hostPtrManager->getFragment(buffer);
    if (fragment && fragment->driverAllocation) {
        OsHandle *osStorageToRelease = fragment->osInternalStorage;
        ResidencyData *residencyDataToRelease = fragment->residency;
        if (hostPtrManager->releaseHostPtr(buffer)) {
            delete osStorageToRelease;
            delete residencyDataToRelease;
        }
    }
}

void OsAgnosticMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) {
    if (gfxAllocation == nullptr)
        return;

    delete gfxAllocation->gmm;

    if ((uintptr_t)gfxAllocation->getUnderlyingBuffer() == dummyAddress) {
        delete gfxAllocation;
        return;
    }

    if (gfxAllocation->fragmentsStorage.fragmentCount) {
        cleanGraphicsMemoryCreatedFromHostPtr(gfxAllocation);
        delete gfxAllocation;
        return;
    }

    if (gfxAllocation->is32BitAllocation) {
        auto gpuAddressToFree = gfxAllocation->getGpuAddress() & ~MemoryConstants::pageMask;
        allocator32Bit->free(gpuAddressToFree, static_cast<MemoryAllocation *>(gfxAllocation)->sizeToFree);
    }

    alignedFreeWrapper(gfxAllocation->driverAllocatedCpuPointer);
    delete gfxAllocation;
}

uint64_t OsAgnosticMemoryManager::getSystemSharedMemory() {
    return 16 * GB;
}

uint64_t OsAgnosticMemoryManager::getMaxApplicationAddress() {
    return MemoryConstants::max32BitAppAddress + static_cast<uint64_t>(is64bit) * (MemoryConstants::max64BitAppAddress - MemoryConstants::max32BitAppAddress);
}

uint64_t OsAgnosticMemoryManager::getInternalHeapBaseAddress() {
    return this->allocator32Bit->getBase();
}

GraphicsAllocation *OsAgnosticMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, size_t hostPtrSize, const void *hostPtr) {
    auto allocation = new MemoryAllocation(nullptr, const_cast<void *>(hostPtr), reinterpret_cast<uint64_t>(hostPtr),
                                           hostPtrSize, counter++, MemoryPool::System4KBPages, this->getOsContextCount(), false);
    allocation->fragmentsStorage = handleStorage;
    return allocation;
}

void OsAgnosticMemoryManager::turnOnFakingBigAllocations() {
    this->fakeBigAllocations = true;
}

MemoryManager::AllocationStatus OsAgnosticMemoryManager::populateOsHandles(OsHandleStorage &handleStorage) {
    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        if (!handleStorage.fragmentStorageData[i].osHandleStorage && handleStorage.fragmentStorageData[i].cpuPtr) {
            handleStorage.fragmentStorageData[i].osHandleStorage = new OsHandle();
            handleStorage.fragmentStorageData[i].residency = new ResidencyData();

            FragmentStorage newFragment = {};
            newFragment.fragmentCpuPointer = const_cast<void *>(handleStorage.fragmentStorageData[i].cpuPtr);
            newFragment.fragmentSize = handleStorage.fragmentStorageData[i].fragmentSize;
            newFragment.osInternalStorage = handleStorage.fragmentStorageData[i].osHandleStorage;
            newFragment.residency = handleStorage.fragmentStorageData[i].residency;
            hostPtrManager->storeFragment(newFragment);
        }
    }
    return AllocationStatus::Success;
}
void OsAgnosticMemoryManager::cleanOsHandles(OsHandleStorage &handleStorage) {
    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        if (handleStorage.fragmentStorageData[i].freeTheFragment) {
            delete handleStorage.fragmentStorageData[i].osHandleStorage;
            delete handleStorage.fragmentStorageData[i].residency;
        }
    }
}
GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryForImage(ImageInfo &imgInfo, const void *hostPtr) {
    GraphicsAllocation *alloc = nullptr;
    auto gmm = std::make_unique<Gmm>(imgInfo);

    alloc = allocateGraphicsMemoryForImageFromHostPtr(imgInfo, hostPtr);

    if (!alloc) {
        if ((!GmmHelper::allowTiling(*imgInfo.imgDesc) && imgInfo.mipCount == 0)) {
            alloc = allocateGraphicsMemoryWithProperties({imgInfo.size, GraphicsAllocation::AllocationType::UNDECIDED});
        } else {
            auto ptr = allocateSystemMemory(alignUp(imgInfo.size, MemoryConstants::pageSize), MemoryConstants::pageSize);
            if (ptr != nullptr) {
                alloc = new MemoryAllocation(ptr, ptr, reinterpret_cast<uint64_t>(ptr), imgInfo.size, counter,
                                             MemoryPool::SystemCpuInaccessible, this->getOsContextCount(), false);
                counter++;
            }
        }
    }

    if (alloc) {
        alloc->gmm = gmm.release();
    }

    return alloc;
}

Allocator32bit *OsAgnosticMemoryManager::create32BitAllocator(bool aubUsage) {
    uint64_t allocatorSize = MemoryConstants::gigaByte - 2 * 4096;
    uint64_t heap32Base = 0x80000000000ul;

    if (is64bit && this->localMemorySupported && aubUsage) {
        heap32Base = 0x40000000000ul;
    }

    if (is32bit) {
        heap32Base = 0x0;
    }

    if (DebugManager.flags.UseMallocToObtainHeap32Base.get()) {
        size_t allocationSize = 40 * MemoryConstants::megaByte;
        allocatorSize = static_cast<uint64_t>(allocationSize);
        heap32Base = castToUint64(alignedMallocWrapper(allocationSize, MemoryConstants::allocationAlignment));
    }

    return new Allocator32bit(heap32Base, allocatorSize);
}
} // namespace OCLRT
