/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/os_agnostic_memory_manager.h"

#include "core/helpers/aligned_memory.h"
#include "core/helpers/basic_math.h"
#include "core/helpers/ptr_math.h"
#include "core/os_interface/os_memory.h"
#include "runtime/aub/aub_center.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/memory_manager/host_ptr_manager.h"

#include <cassert>

namespace NEO {

OsAgnosticMemoryManager::OsAgnosticMemoryManager(bool aubUsage, ExecutionEnvironment &executionEnvironment) : MemoryManager(executionEnvironment) {
    auto gpuAddressSpace = executionEnvironment.getHardwareInfo()->capabilityTable.gpuAddressSpace;

    // 4 x sizeof(Heap32) + 2 x sizeof(Standard/Standard64k)
    size_t reservedCpuAddressRangeSize = is64bit ? (4 * 4 + 2 * (aubUsage ? 32 : 4)) * GB : 0;
    gfxPartition->init(gpuAddressSpace, reservedCpuAddressRangeSize);
}

OsAgnosticMemoryManager::~OsAgnosticMemoryManager() {
    applyCommonCleanup();
}

struct OsHandle {
};

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {
    auto sizeAligned = alignUp(allocationData.size, MemoryConstants::pageSize);
    MemoryAllocation *memoryAllocation = nullptr;

    if (fakeBigAllocations && allocationData.size > bigAllocation) {
        memoryAllocation = createMemoryAllocation(
            allocationData.type, nullptr, (void *)dummyAddress, static_cast<uint64_t>(dummyAddress), allocationData.size, counter,
            MemoryPool::System4KBPages, allocationData.flags.multiOsContextCapable, allocationData.flags.uncacheable,
            allocationData.flags.flushL3, false);
        counter++;
        return memoryAllocation;
    }
    auto ptr = allocateSystemMemory(sizeAligned, allocationData.alignment ? alignUp(allocationData.alignment, MemoryConstants::pageSize) : MemoryConstants::pageSize);
    if (ptr != nullptr) {
        memoryAllocation = createMemoryAllocation(allocationData.type, ptr, ptr, reinterpret_cast<uint64_t>(ptr), allocationData.size,
                                                  counter, MemoryPool::System4KBPages, allocationData.flags.multiOsContextCapable,
                                                  allocationData.flags.uncacheable, allocationData.flags.flushL3, false);

        if (allocationData.type == GraphicsAllocation::AllocationType::SVM_CPU) {
            //add 2MB padding in case mapPtr is not 2MB aligned
            size_t reserveSize = sizeAligned + allocationData.alignment;
            void *gpuPtr = reserveCpuAddressRange(reserveSize);
            if (!gpuPtr) {
                delete memoryAllocation;
                alignedFreeWrapper(ptr);
                return nullptr;
            }
            memoryAllocation->setReservedAddressRange(gpuPtr, reserveSize);
            gpuPtr = alignUp(gpuPtr, allocationData.alignment);
            memoryAllocation->setCpuPtrAndGpuAddress(ptr, reinterpret_cast<uint64_t>(gpuPtr));
        }
    }
    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) {
    auto alignedPtr = alignDown(allocationData.hostPtr, MemoryConstants::pageSize);
    auto offsetInPage = ptrDiff(allocationData.hostPtr, alignedPtr);

    auto memoryAllocation = createMemoryAllocation(allocationData.type, nullptr, const_cast<void *>(allocationData.hostPtr),
                                                   reinterpret_cast<uint64_t>(alignedPtr), allocationData.size, counter,
                                                   MemoryPool::System4KBPages, false, false, allocationData.flags.flushL3, false);

    memoryAllocation->setAllocationOffset(offsetInPage);

    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemory64kb(const AllocationData &allocationData) {
    AllocationData allocationData64kb = allocationData;
    allocationData64kb.size = alignUp(allocationData.size, MemoryConstants::pageSize64k);
    allocationData64kb.alignment = MemoryConstants::pageSize64k;
    auto memoryAllocation = allocateGraphicsMemoryWithAlignment(allocationData64kb);
    if (memoryAllocation) {
        static_cast<MemoryAllocation *>(memoryAllocation)->overrideMemoryPool(MemoryPool::System64KBPages);
    }
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) {
    auto heap = useInternal32BitAllocator(allocationData.type) ? internalHeapIndex : HeapIndex::HEAP_EXTERNAL;
    if (allocationData.hostPtr) {
        auto allocationSize = alignSizeWholePage(allocationData.hostPtr, allocationData.size);
        auto gpuVirtualAddress = gfxPartition->heapAllocate(heap, allocationSize);
        if (!gpuVirtualAddress) {
            return nullptr;
        }
        uint64_t offset = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(allocationData.hostPtr) & MemoryConstants::pageMask);
        MemoryAllocation *memAlloc = new MemoryAllocation(
            allocationData.type, nullptr, const_cast<void *>(allocationData.hostPtr), GmmHelper::canonize(gpuVirtualAddress + offset),
            allocationData.size, counter, MemoryPool::System4KBPagesWith32BitGpuAddressing, false, false, false);

        memAlloc->set32BitAllocation(true);
        memAlloc->setGpuBaseAddress(GmmHelper::canonize(gfxPartition->getHeapBase(heap)));
        memAlloc->sizeToFree = allocationSize;

        counter++;
        return memAlloc;
    }

    auto allocationSize = alignUp(allocationData.size, MemoryConstants::pageSize);
    void *ptrAlloc = nullptr;
    auto gpuAddress = gfxPartition->heapAllocate(heap, allocationSize);

    if (allocationData.size < 0xfffff000) {
        if (fakeBigAllocations) {
            ptrAlloc = reinterpret_cast<void *>(dummyAddress);
        } else {
            ptrAlloc = alignedMallocWrapper(allocationSize, MemoryConstants::allocationAlignment);
        }
    }

    MemoryAllocation *memoryAllocation = nullptr;
    if (ptrAlloc != nullptr) {
        memoryAllocation = new MemoryAllocation(allocationData.type, ptrAlloc, ptrAlloc, GmmHelper::canonize(gpuAddress),
                                                allocationData.size, counter, MemoryPool::System4KBPagesWith32BitGpuAddressing, false,
                                                false, allocationData.flags.flushL3);

        memoryAllocation->set32BitAllocation(true);
        memoryAllocation->setGpuBaseAddress(GmmHelper::canonize(gfxPartition->getHeapBase(heap)));
        memoryAllocation->sizeToFree = allocationSize;
    }
    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness) {
    auto graphicsAllocation = createMemoryAllocation(properties.allocationType, nullptr, reinterpret_cast<void *>(1), 1,
                                                     4096u, static_cast<uint64_t>(handle), MemoryPool::SystemCpuInaccessible, false,
                                                     false, false, requireSpecificBitness);
    graphicsAllocation->setSharedHandle(handle);
    graphicsAllocation->set32BitAllocation(requireSpecificBitness);

    if (properties.imgInfo) {
        Gmm *gmm = new Gmm(*properties.imgInfo, createStorageInfoFromProperties(properties));
        graphicsAllocation->setDefaultGmm(gmm);
    }

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
    for (auto handleId = 0u; handleId < maxHandleCount; handleId++) {
        delete gfxAllocation->getGmm(handleId);
    }

    if ((uintptr_t)gfxAllocation->getUnderlyingBuffer() == dummyAddress) {
        delete gfxAllocation;
        return;
    }

    if (gfxAllocation->fragmentsStorage.fragmentCount) {
        cleanGraphicsMemoryCreatedFromHostPtr(gfxAllocation);
        delete gfxAllocation;
        return;
    }

    auto memoryAllocation = static_cast<MemoryAllocation *>(gfxAllocation);
    auto sizeToFree = memoryAllocation->sizeToFree;

    if (sizeToFree) {
        auto gpuAddressToFree = GmmHelper::decanonize(memoryAllocation->getGpuAddress()) & ~MemoryConstants::pageMask;
        gfxPartition->freeGpuAddressRange(gpuAddressToFree, sizeToFree);
    }

    alignedFreeWrapper(gfxAllocation->getDriverAllocatedCpuPtr());
    if (gfxAllocation->getReservedAddressPtr()) {
        releaseReservedCpuAddressRange(gfxAllocation->getReservedAddressPtr(), gfxAllocation->getReservedAddressSize());
    }

    auto aubCenter = executionEnvironment.aubCenter.get();
    if (aubCenter && aubCenter->getAubManager() && DebugManager.flags.EnableFreeMemory.get()) {
        aubCenter->getAubManager()->freeMemory(gfxAllocation->getGpuAddress(), gfxAllocation->getUnderlyingBufferSize());
    }

    delete gfxAllocation;
}

uint64_t OsAgnosticMemoryManager::getSystemSharedMemory() {
    return 16 * GB;
}

GraphicsAllocation *OsAgnosticMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) {
    auto allocation = createMemoryAllocation(allocationData.type, nullptr, const_cast<void *>(allocationData.hostPtr),
                                             reinterpret_cast<uint64_t>(allocationData.hostPtr), allocationData.size, counter++,
                                             MemoryPool::System4KBPages, false, false, false, false);

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
            auto aubCenter = executionEnvironment.aubCenter.get();
            if (aubCenter && aubCenter->getAubManager() && DebugManager.flags.EnableFreeMemory.get()) {
                aubCenter->getAubManager()->freeMemory((uint64_t)handleStorage.fragmentStorageData[i].cpuPtr, handleStorage.fragmentStorageData[i].fragmentSize);
            }
            delete handleStorage.fragmentStorageData[i].osHandleStorage;
            delete handleStorage.fragmentStorageData[i].residency;
        }
    }
}
GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) {
    GraphicsAllocation *alloc = nullptr;

    if (allocationData.imgInfo->linearStorage && allocationData.imgInfo->mipCount == 0) {
        alloc = allocateGraphicsMemoryWithAlignment(allocationData);
        alloc->setDefaultGmm(gmm.release());
        return alloc;
    }

    auto ptr = allocateSystemMemory(alignUp(allocationData.imgInfo->size, MemoryConstants::pageSize), MemoryConstants::pageSize);
    if (ptr != nullptr) {
        alloc = createMemoryAllocation(allocationData.type, ptr, ptr, reinterpret_cast<uint64_t>(ptr), allocationData.imgInfo->size,
                                       counter, MemoryPool::SystemCpuInaccessible, false, allocationData.flags.uncacheable, allocationData.flags.flushL3, false);
        counter++;
    }

    if (alloc) {
        alloc->setDefaultGmm(gmm.release());
    }

    return alloc;
}

void *OsAgnosticMemoryManager::reserveCpuAddressRange(size_t size) {
    void *reservePtr = allocateSystemMemory(size, MemoryConstants::preferredAlignment);
    return reservePtr;
}

void OsAgnosticMemoryManager::releaseReservedCpuAddressRange(void *reserved, size_t size) {
    alignedFreeWrapper(reserved);
}

MemoryAllocation *OsAgnosticMemoryManager::createMemoryAllocation(GraphicsAllocation::AllocationType allocationType, void *driverAllocatedCpuPointer,
                                                                  void *pMem, uint64_t gpuAddress, size_t memSize, uint64_t count,
                                                                  MemoryPool::Type pool, bool multiOsContextCapable, bool uncacheable,
                                                                  bool flushL3Required, bool requireSpecificBitness) {
    if (!isLimitedRange()) {
        return new MemoryAllocation(allocationType, driverAllocatedCpuPointer, pMem, gpuAddress, memSize,
                                    count, pool, multiOsContextCapable, uncacheable, flushL3Required);
    }

    size_t alignedSize = alignSizeWholePage(pMem, memSize);

    auto heap = (force32bitAllocations || requireSpecificBitness) ? HeapIndex::HEAP_EXTERNAL : HeapIndex::HEAP_STANDARD;

    uint64_t limitedGpuAddress = gfxPartition->heapAllocate(heap, alignedSize);

    auto memoryAllocation = new MemoryAllocation(allocationType, driverAllocatedCpuPointer, pMem, limitedGpuAddress, memSize,
                                                 count, pool, multiOsContextCapable, uncacheable, flushL3Required);

    if (heap == HeapIndex::HEAP_EXTERNAL) {
        memoryAllocation->setGpuBaseAddress(GmmHelper::canonize(gfxPartition->getHeapBase(heap)));
    }
    memoryAllocation->sizeToFree = alignedSize;

    return memoryAllocation;
}
} // namespace NEO
