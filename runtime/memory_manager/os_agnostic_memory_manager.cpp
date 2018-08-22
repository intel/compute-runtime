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

#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/surface_formats.h"
#include <cassert>

namespace OCLRT {

OsAgnosticMemoryManager::~OsAgnosticMemoryManager() {
    applyCommonCleanup();
}

struct OsHandle {
};

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) {

    auto sizeAligned = alignUp(size, MemoryConstants::pageSize);
    MemoryAllocation *memoryAllocation = nullptr;

    if (fakeBigAllocations && size > bigAllocation) {
        memoryAllocation = new MemoryAllocation(true, (void *)dummyAddress, static_cast<uint64_t>(dummyAddress), size, counter, MemoryPool::System4KBPages);
        counter++;
        memoryAllocation->uncacheable = uncacheable;
        return memoryAllocation;
    }
    auto ptr = allocateSystemMemory(sizeAligned, alignment ? alignUp(alignment, MemoryConstants::pageSize) : MemoryConstants::pageSize);
    if (ptr != nullptr) {
        memoryAllocation = new MemoryAllocation(true, ptr, reinterpret_cast<uint64_t>(ptr), size, counter, MemoryPool::System4KBPages);
        if (!memoryAllocation) {
            alignedFreeWrapper(ptr);
            return nullptr;
        }
        memoryAllocation->uncacheable = uncacheable;
    }
    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin, bool preferRenderCompressed) {
    auto memoryAllocation = allocateGraphicsMemory(alignUp(size, MemoryConstants::pageSize64k), MemoryConstants::pageSize64k, forcePin, false);
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
        MemoryAllocation *memAlloc = new MemoryAllocation(false, const_cast<void *>(ptr), GmmHelper::canonize(gpuVirtualAddress + offset), size, counter, MemoryPool::System4KBPagesWith32BitGpuAddressing);
        memAlloc->is32BitAllocation = true;
        memAlloc->gpuBaseAddress = GmmHelper::canonize(allocator32Bit->getBase());
        memAlloc->sizeToFree = allocationSize;

        counter++;
        return memAlloc;
    }

    auto allocationSize = alignUp(size, MemoryConstants::pageSize);
    void *ptrAlloc = nullptr;

    if (size < 0xfffff000)
        ptrAlloc = alignedMallocWrapper(allocationSize, MemoryConstants::allocationAlignment);
    auto gpuAddress = allocator32Bit->allocate(allocationSize);

    MemoryAllocation *memoryAllocation = nullptr;
    if (ptrAlloc != nullptr) {
        memoryAllocation = new MemoryAllocation(true, ptrAlloc, GmmHelper::canonize(gpuAddress), size, counter, MemoryPool::System4KBPagesWith32BitGpuAddressing);
        memoryAllocation->is32BitAllocation = true;
        memoryAllocation->gpuBaseAddress = GmmHelper::canonize(allocator32Bit->getBase());
        memoryAllocation->sizeToFree = allocationSize;
        memoryAllocation->cpuPtrAllocated = true;
    }
    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness, bool reuseBO) {
    auto graphicsAllocation = new MemoryAllocation(false, reinterpret_cast<void *>(1), 1, 4096u, static_cast<uint64_t>(handle), MemoryPool::SystemCpuInaccessible);
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
    hostPtrManager.storeFragment(fragment);
}

void OsAgnosticMemoryManager::removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) {
    auto buffer = gfxAllocation->getUnderlyingBuffer();
    auto fragment = hostPtrManager.getFragment(buffer);
    if (fragment && fragment->driverAllocation) {
        OsHandle *osStorageToRelease = fragment->osInternalStorage;
        if (hostPtrManager.releaseHostPtr(buffer)) {
            delete osStorageToRelease;
        }
    }
}

void OsAgnosticMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) {
    if (gfxAllocation == nullptr)
        return;
    if (gfxAllocation->gmm)
        freeGmm(gfxAllocation);
    if ((uintptr_t)gfxAllocation->getUnderlyingBuffer() == dummyAddress) {
        delete gfxAllocation;
        return;
    }

    if (gfxAllocation->fragmentsStorage.fragmentCount) {
        cleanGraphicsMemoryCreatedFromHostPtr(gfxAllocation);
        delete gfxAllocation;
        return;
    }

    void *ptr = gfxAllocation->getUnderlyingBuffer();

    if (gfxAllocation->is32BitAllocation) {
        auto gpuAddressToFree = gfxAllocation->getGpuAddress() & ~MemoryConstants::pageMask;
        allocator32Bit->free(gpuAddressToFree, static_cast<MemoryAllocation *>(gfxAllocation)->sizeToFree);
    }
    if (gfxAllocation->cpuPtrAllocated) {
        alignedFreeWrapper(ptr);
    }
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
    auto allocation = new MemoryAllocation(false, const_cast<void *>(hostPtr), reinterpret_cast<uint64_t>(hostPtr), hostPtrSize, counter++, MemoryPool::System4KBPages);
    allocation->fragmentsStorage = handleStorage;
    return allocation;
}

void OsAgnosticMemoryManager::turnOnFakingBigAllocations() {
    this->fakeBigAllocations = true;
}

MemoryManager::AllocationStatus OsAgnosticMemoryManager::populateOsHandles(OsHandleStorage &handleStorage) {
    for (unsigned int i = 0; i < max_fragments_count; i++) {
        if (!handleStorage.fragmentStorageData[i].osHandleStorage && handleStorage.fragmentStorageData[i].cpuPtr) {
            handleStorage.fragmentStorageData[i].osHandleStorage = new OsHandle();
            handleStorage.fragmentStorageData[i].residency = new ResidencyData();

            FragmentStorage newFragment = {};
            newFragment.fragmentCpuPointer = const_cast<void *>(handleStorage.fragmentStorageData[i].cpuPtr);
            newFragment.fragmentSize = handleStorage.fragmentStorageData[i].fragmentSize;
            newFragment.osInternalStorage = handleStorage.fragmentStorageData[i].osHandleStorage;
            newFragment.residency = handleStorage.fragmentStorageData[i].residency;
            hostPtrManager.storeFragment(newFragment);
        }
    }
    return AllocationStatus::Success;
}
void OsAgnosticMemoryManager::cleanOsHandles(OsHandleStorage &handleStorage) {
    for (unsigned int i = 0; i < max_fragments_count; i++) {
        if (handleStorage.fragmentStorageData[i].freeTheFragment) {
            delete handleStorage.fragmentStorageData[i].osHandleStorage;
            delete handleStorage.fragmentStorageData[i].residency;
        }
    }
}
GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) {
    GraphicsAllocation *alloc = nullptr;
    if (!GmmHelper::allowTiling(*imgInfo.imgDesc) && imgInfo.mipCount == 0) {
        alloc = allocateGraphicsMemory(imgInfo.size);
    } else {
        auto ptr = allocateSystemMemory(alignUp(imgInfo.size, MemoryConstants::pageSize), MemoryConstants::pageSize);
        if (ptr != nullptr) {
            alloc = new MemoryAllocation(true, ptr, reinterpret_cast<uint64_t>(ptr), imgInfo.size, counter, MemoryPool::SystemCpuInaccessible);
            counter++;
        }
    }
    if (alloc) {
        alloc->gmm = gmm;
    }

    return alloc;
}
} // namespace OCLRT
