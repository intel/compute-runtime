/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
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
        memoryAllocation = new MemoryAllocation(true, 1, (void *)dummyAddress, static_cast<uint64_t>(dummyAddress), size, counter);
        counter++;
        memoryAllocation->dummyAllocation = true;
        memoryAllocation->uncacheable = uncacheable;
        return memoryAllocation;
    }
    auto ptr = allocateSystemMemory(sizeAligned, alignment ? alignUp(alignment, MemoryConstants::pageSize) : MemoryConstants::pageSize);
    DEBUG_BREAK_IF(allocationMap.find(ptr) != allocationMap.end());
    if (ptr != nullptr) {
        memoryAllocation = new MemoryAllocation(true, 1, ptr, reinterpret_cast<uint64_t>(ptr), size, counter);
        if (!memoryAllocation) {
            alignedFreeWrapper(ptr);
            return nullptr;
        }
        memoryAllocation->uncacheable = uncacheable;
        allocationMap.emplace(ptr, memoryAllocation);
    }
    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin) {
    return allocateGraphicsMemory(alignUp(size, MemoryConstants::pageSize64k), MemoryConstants::pageSize64k, forcePin, false);
}

GraphicsAllocation *OsAgnosticMemoryManager::allocate32BitGraphicsMemory(size_t size, void *ptr, MemoryType memoryType) {
    if (ptr) {
        auto allocationSize = alignSizeWholePage(reinterpret_cast<void *>(ptr), size);
        auto gpuVirtualAddress = allocator32Bit->allocate(allocationSize);
        if (!gpuVirtualAddress) {
            return nullptr;
        }
        uint64_t offset = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr) & MemoryConstants::pageMask);
        MemoryAllocation *memAlloc = new MemoryAllocation(false, 1, reinterpret_cast<void *>(ptr), Gmm::canonize(reinterpret_cast<uint64_t>(gpuVirtualAddress) + offset), size, counter);
        memAlloc->is32BitAllocation = true;
        memAlloc->gpuBaseAddress = Gmm::canonize(allocator32Bit->getBase());
        memAlloc->sizeToFree = allocationSize;

        allocationMap.emplace(const_cast<void *>(ptr), memAlloc);
        counter++;
        return memAlloc;
    }

    auto allocationSize = alignUp(size, MemoryConstants::pageSize);
    void *ptrAlloc = nullptr;

    if (size < 0xfffff000)
        ptrAlloc = alignedMallocWrapper(allocationSize, MemoryConstants::allocationAlignment);
    void *gpuPointer = allocator32Bit->allocate(allocationSize);

    DEBUG_BREAK_IF(allocationMap.find(ptrAlloc) != allocationMap.end());
    MemoryAllocation *memoryAllocation = nullptr;
    if (ptrAlloc != nullptr) {
        memoryAllocation = new MemoryAllocation(true, 1, ptrAlloc, Gmm::canonize(reinterpret_cast<uint64_t>(gpuPointer)), size, counter);
        memoryAllocation->is32BitAllocation = true;
        memoryAllocation->gpuBaseAddress = Gmm::canonize(allocator32Bit->getBase());
        memoryAllocation->sizeToFree = allocationSize;
        memoryAllocation->cpuPtrAllocated = true;
        allocationMap.emplace(ptrAlloc, memoryAllocation);
    }
    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness, bool reuseBO) {
    auto graphicsAllocation = new MemoryAllocation(false, 1, reinterpret_cast<void *>(1), 1, 4096u, static_cast<uint64_t>(handle));
    graphicsAllocation->setSharedHandle(handle);
    graphicsAllocation->is32BitAllocation = requireSpecificBitness;
    return graphicsAllocation;
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

    bool freeMemory = false;
    bool is32BitAllocation = false;
    void *ptr = gfxAllocation->getUnderlyingBuffer();
    void *gpuPtrToFree = nullptr;
    size_t sizeToFree = 0;
    auto it = allocationMap.find(ptr);

    if (it != allocationMap.end()) {
        it->second->refCount--;
        if (it->second->refCount == 0) {
            freeMemory = it->second->cpuPtrAllocated;
            is32BitAllocation = it->second->is32BitAllocation;
            gpuPtrToFree = reinterpret_cast<void *>(it->second->getGpuAddress() & ~MemoryConstants::pageMask);
            sizeToFree = it->second->sizeToFree;
            allocationMap.erase(it);
        }
    }
    if (is32BitAllocation) {
        allocator32Bit->free(gpuPtrToFree, sizeToFree);
        if (freeMemory) {
            alignedFreeWrapper(ptr);
        }
    } else if (freeMemory) {
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

GraphicsAllocation *OsAgnosticMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, size_t hostPtrSize, const void *hostPtr) {
    auto allocation = new MemoryAllocation(false, 0, const_cast<void *>(hostPtr), reinterpret_cast<uint64_t>(hostPtr), hostPtrSize, counter++);
    allocation->fragmentsStorage = handleStorage;
    return allocation;
}

void OsAgnosticMemoryManager::turnOnFakingBigAllocations() {
    this->fakeBigAllocations = true;
}

bool OsAgnosticMemoryManager::populateOsHandles(OsHandleStorage &handleStorage) {
    for (unsigned int i = 0; i < max_fragments_count; i++) {
        if (!handleStorage.fragmentStorageData[i].osHandleStorage && handleStorage.fragmentStorageData[i].cpuPtr) {
            handleStorage.fragmentStorageData[i].osHandleStorage = new OsHandle();
            handleStorage.fragmentStorageData[i].residency = new ResidencyData();

            FragmentStorage newFragment;
            newFragment.fragmentCpuPointer = const_cast<void *>(handleStorage.fragmentStorageData[i].cpuPtr);
            newFragment.fragmentSize = handleStorage.fragmentStorageData[i].fragmentSize;
            newFragment.osInternalStorage = handleStorage.fragmentStorageData[i].osHandleStorage;
            newFragment.residency = handleStorage.fragmentStorageData[i].residency;
            hostPtrManager.storeFragment(newFragment);
        }
    }
    return true;
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
    auto alloc = allocateGraphicsMemory(imgInfo.size, MemoryConstants::preferredAlignment);
    alloc->gmm = gmm;

    return alloc;
}
} // namespace OCLRT
