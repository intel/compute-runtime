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

#include "runtime/device/device.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/32bit_memory.h"
#include "runtime/os_interface/linux/drm_allocation.h"
#include "runtime/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/helpers/surface_formats.h"
#include <cstring>
#include <iostream>

extern "C" {
#include "drm/i915_drm.h"
#include "drm/drm.h"
}

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"

namespace OCLRT {

DrmMemoryManager::DrmMemoryManager(Drm *drm, gemCloseWorkerMode mode, bool forcePinAllowed) : MemoryManager(false), drm(drm), pinBB(nullptr) {
    MemoryManager::virtualPaddingAvailable = true;
    if (mode != gemCloseWorkerMode::gemCloseWorkerInactive) {
        gemCloseWorker.reset(new DrmGemCloseWorker(*this));
    }

    if (forcePinAllowed) {
        auto mem = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
        DEBUG_BREAK_IF(mem == nullptr);

        pinBB = allocUserptr(reinterpret_cast<uintptr_t>(mem), MemoryConstants::pageSize, 0, true);

        if (!pinBB) {
            alignedFree(mem);
        } else {
            pinBB->isAllocated = true;
        }
    }
    internal32bitAllocator.reset(new Allocator32bit);
}

DrmMemoryManager::~DrmMemoryManager() {
    applyCommonCleanup();
    if (gemCloseWorker) {
        gemCloseWorker->close(false);
    }
    if (pinBB) {
        unreference(pinBB);
        pinBB = nullptr;
    }
}

void DrmMemoryManager::push(DrmAllocation *alloc) {
    gemCloseWorker->push(alloc);
}

void DrmMemoryManager::eraseSharedBufferObject(OCLRT::BufferObject *bo) {
    std::lock_guard<decltype(mtx)> lock(mtx);

    auto it = std::find(sharingBufferObjects.begin(), sharingBufferObjects.end(), bo);
    //If an object isReused = true, it must be in the vector
    DEBUG_BREAK_IF(it == sharingBufferObjects.end());
    sharingBufferObjects.erase(it);
}

void DrmMemoryManager::pushSharedBufferObject(OCLRT::BufferObject *bo) {
    std::lock_guard<decltype(mtx)> lock(mtx);

    bo->isReused = true;
    sharingBufferObjects.push_back(bo);
}

uint32_t DrmMemoryManager::unreference(OCLRT::BufferObject *bo, bool synchronousDestroy) {
    if (!bo)
        return -1;

    if (synchronousDestroy) {
        while (bo->refCount > 1)
            ;
    }

    uint32_t r = bo->refCount.fetch_sub(1);

    if (r == 1) {
        for (auto it : *bo->getResidency()) {
            unreference(it);
        }
        auto unmapSize = bo->peekUnmapSize();
        auto address = bo->isAllocated || unmapSize > 0 ? bo->address : nullptr;
        auto allocatorType = bo->peekAllocationType();

        if (bo->isReused) {
            eraseSharedBufferObject(bo);
        }

        bo->close();

        delete bo;
        if (address) {
            if (unmapSize) {
                if (allocatorType == MMAP_ALLOCATOR) {
                    munmapFunction(address, unmapSize);
                } else {
                    allocator32Bit->free(address, unmapSize);
                }

            } else {
                alignedFreeWrapper(address);
            }
        }
    }
    return r;
}

OCLRT::BufferObject *DrmMemoryManager::allocUserptr(uintptr_t address, size_t size, uint64_t flags, bool softpin) {
    struct drm_i915_gem_userptr userptr;

    memset(&userptr, 0, sizeof(userptr));
    userptr.user_ptr = address;
    userptr.user_size = size;
    userptr.flags = static_cast<uint32_t>(flags);

    int ret = this->drm->ioctl(DRM_IOCTL_I915_GEM_USERPTR,
                               &userptr);
    if (ret != 0)
        return nullptr;

    auto res = new (std::nothrow) BufferObject(this->drm, userptr.handle, false);
    if (!res) {
        DEBUG_BREAK_IF(true);
        return nullptr;
    }
    res->size = size;
    res->address = reinterpret_cast<void *>(address);
    res->softPin(address);

    return res;
}

DrmAllocation *DrmMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, size_t hostPtrSize, const void *hostPtr) {
    auto allocation = new DrmAllocation(nullptr, const_cast<void *>(hostPtr), hostPtrSize);
    allocation->fragmentsStorage = handleStorage;
    return allocation;
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) {
    const size_t minAlignment = MemoryConstants::allocationAlignment;
    size_t cAlignment = alignUp(std::max(alignment, minAlignment), minAlignment);
    // When size == 0 allocate allocationAlignment
    // It's needed to prevent overlapping pages with user pointers
    size_t cSize = std::max(alignUp(size, minAlignment), minAlignment);

    auto res = alignedMallocWrapper(cSize, cAlignment);

    if (!res)
        return nullptr;

    BufferObject *bo = allocUserptr(reinterpret_cast<uintptr_t>(res), cSize, 0, true);

    if (!bo) {
        alignedFreeWrapper(res);
        return nullptr;
    }

    bo->isAllocated = true;
    if (pinBB != nullptr && forcePin && size >= this->pinThreshold) {
        pinBB->pin(bo);
    }

    return new DrmAllocation(bo, res, cSize);
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemory(size_t size, const void *ptr, bool forcePin) {
    auto res = (DrmAllocation *)MemoryManager::allocateGraphicsMemory(size, const_cast<void *>(ptr));

    if (res != nullptr && pinBB != nullptr && forcePin && size >= this->pinThreshold) {
        pinBB->pin(res->getBO());
    }

    return res;
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin) {
    return nullptr;
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) {
    if (!Gmm::allowTiling(*imgInfo.imgDesc)) {
        auto alloc = allocateGraphicsMemory(imgInfo.size, MemoryConstants::preferredAlignment);
        if (alloc) {
            alloc->gmm = gmm;
        }
        return alloc;
    }

    auto gpuRange = mmapFunction(nullptr, imgInfo.size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    DEBUG_BREAK_IF(gpuRange == MAP_FAILED);

    drm_i915_gem_create create = {0, 0, 0};
    create.size = imgInfo.size;

    auto ret = this->drm->ioctl(DRM_IOCTL_I915_GEM_CREATE, &create);
    DEBUG_BREAK_IF(ret != 0);
    ((void)(ret));

    auto bo = new (std::nothrow) BufferObject(this->drm, create.handle, true);
    if (!bo) {
        return nullptr;
    }
    bo->size = imgInfo.size;
    bo->address = reinterpret_cast<void *>(gpuRange);
    bo->softPin(reinterpret_cast<uint64_t>(gpuRange));

    auto ret2 = bo->setTiling(I915_TILING_Y, static_cast<uint32_t>(imgInfo.rowPitch));
    DEBUG_BREAK_IF(ret2 != true);
    ((void)(ret2));

    bo->setUnmapSize(imgInfo.size);

    auto allocation = new DrmAllocation(bo, gpuRange, imgInfo.size);
    bo->setAllocationType(MMAP_ALLOCATOR);
    allocation->gmm = gmm;
    return allocation;
}

DrmAllocation *DrmMemoryManager::allocate32BitGraphicsMemory(size_t size, void *ptr, MemoryType memoryType) {
    if (ptr) {
        uintptr_t inputPtr = (uintptr_t)ptr;
        auto allocationSize = alignSizeWholePage((void *)ptr, size);
        auto realAllocationSize = allocationSize;
        auto gpuVirtualAddress = allocator32Bit->allocate(realAllocationSize);
        if (!gpuVirtualAddress) {
            return nullptr;
        }
        auto alignedUserPointer = (uintptr_t)alignDown(ptr, MemoryConstants::pageSize);
        auto inputPointerOffset = inputPtr - alignedUserPointer;

        BufferObject *bo = allocUserptr(alignedUserPointer, allocationSize, 0, true);
        if (!bo) {
            allocator32Bit->free(gpuVirtualAddress, realAllocationSize);
            return nullptr;
        }

        bo->isAllocated = false;
        bo->setUnmapSize(realAllocationSize);
        bo->address = gpuVirtualAddress;
        uintptr_t offset = (uintptr_t)bo->address;
        bo->softPin((uint64_t)offset);
        auto drmAllocation = new DrmAllocation(bo, (void *)ptr, (uint64_t)ptrOffset(gpuVirtualAddress, inputPointerOffset), allocationSize);
        drmAllocation->is32BitAllocation = true;
        drmAllocation->gpuBaseAddress = allocator32Bit->getBase();
        return drmAllocation;
    }

    size_t alignedAllocationSize = alignUp(size, MemoryConstants::pageSize);
    auto allocationSize = alignedAllocationSize;
    auto res = allocator32Bit->allocate(allocationSize);

    if (!res) {
        if (device && device->getProgramCount() == 0) {
            this->force32bitAllocations = false;
            device->setForce32BitAddressing(false);
            return (DrmAllocation *)createGraphicsAllocationWithRequiredBitness(size, ptr);
        }

        return nullptr;
    }

    BufferObject *bo = allocUserptr(reinterpret_cast<uintptr_t>(res), alignedAllocationSize, 0, true);

    if (!bo) {
        allocator32Bit->free(res, allocationSize);
        return nullptr;
    }

    bo->isAllocated = true;
    bo->setUnmapSize(allocationSize);

    auto drmAllocation = new DrmAllocation(bo, res, alignedAllocationSize);
    drmAllocation->is32BitAllocation = true;
    drmAllocation->gpuBaseAddress = allocator32Bit->getBase();
    return drmAllocation;
}

BufferObject *DrmMemoryManager::findAndReferenceSharedBufferObject(int boHandle) {
    BufferObject *bo = nullptr;

    std::lock_guard<decltype(mtx)> lock(mtx);
    for (const auto &i : sharingBufferObjects) {
        if (i->handle == static_cast<int>(boHandle)) {
            bo = i;
            bo->reference();
            break;
        }
    }

    return bo;
}

BufferObject *DrmMemoryManager::createSharedBufferObject(int boHandle, size_t size, bool requireSpecificBitness) {
    void *gpuRange = nullptr;
    StorageAllocatorType storageType = UNKNOWN_ALLOCATOR;

    if (requireSpecificBitness && this->force32bitAllocations) {
        gpuRange = this->allocator32Bit->allocate(size);
        storageType = BIT32_ALLOCATOR;
    } else {
        gpuRange = mmapFunction(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        storageType = MMAP_ALLOCATOR;
    }

    DEBUG_BREAK_IF(gpuRange == MAP_FAILED);

    auto bo = new (std::nothrow) BufferObject(this->drm, boHandle, true);
    if (!bo) {
        return nullptr;
    }

    bo->size = size;
    bo->address = reinterpret_cast<void *>(gpuRange);
    bo->softPin(reinterpret_cast<uint64_t>(gpuRange));
    bo->setUnmapSize(size);
    bo->setAllocationType(storageType);
    return bo;
}

GraphicsAllocation *DrmMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness, bool reuseBO) {

    drm_prime_handle openFd = {0, 0, 0};
    openFd.fd = handle;
    auto ret = this->drm->ioctl(DRM_IOCTL_PRIME_FD_TO_HANDLE, &openFd);
    DEBUG_BREAK_IF(ret != 0);
    ((void)(ret));

    auto boHandle = openFd.handle;
    BufferObject *bo = nullptr;

    if (reuseBO) {
        bo = findAndReferenceSharedBufferObject(boHandle);
    }

    if (bo == nullptr) {
        size_t size = lseekFunction(handle, 0, SEEK_END);
        bo = createSharedBufferObject(boHandle, size, requireSpecificBitness);

        if (!bo) {
            return nullptr;
        }

        if (reuseBO) {
            pushSharedBufferObject(bo);
        }
    }

    auto drmAllocation = new DrmAllocation(bo, bo->address, bo->size, handle);

    if (requireSpecificBitness && this->force32bitAllocations) {
        drmAllocation->is32BitAllocation = true;
        drmAllocation->gpuBaseAddress = allocator32Bit->getBase();
    }

    return drmAllocation;
}

GraphicsAllocation *DrmMemoryManager::createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) {
    void *gpuRange = mmapFunction(nullptr, sizeWithPadding, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

    auto srcPtr = inputGraphicsAllocation->getUnderlyingBuffer();
    auto srcSize = inputGraphicsAllocation->getUnderlyingBufferSize();
    auto alignedSrcSize = alignUp(srcSize, MemoryConstants::pageSize);
    auto alignedPtr = (uintptr_t)alignDown(srcPtr, MemoryConstants::pageSize);
    auto offset = (uintptr_t)srcPtr - alignedPtr;

    BufferObject *bo = allocUserptr(alignedPtr, alignedSrcSize, 0, true);
    if (!bo) {
        return nullptr;
    }
    bo->setAddress(gpuRange);
    bo->softPin(reinterpret_cast<uint64_t>(gpuRange));
    bo->setUnmapSize(sizeWithPadding);
    bo->setAllocationType(MMAP_ALLOCATOR);
    return new DrmAllocation(bo, (void *)srcPtr, (uint64_t)ptrOffset(gpuRange, offset), sizeWithPadding);
}

void DrmMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) {
    DrmAllocation *input;
    input = static_cast<DrmAllocation *>(gfxAllocation);
    if (input == nullptr)
        return;
    if (input->gmm)
        delete input->gmm;

    if (gfxAllocation->fragmentsStorage.fragmentCount) {
        cleanGraphicsMemoryCreatedFromHostPtr(gfxAllocation);
        delete gfxAllocation;
        return;
    }

    BufferObject *search = input->getBO();

    if (gfxAllocation->peekSharedHandle() != Sharing::nonSharedResource) {
        closeFunction(gfxAllocation->peekSharedHandle());
    }

    delete gfxAllocation;

    search->wait(-1);
    unreference(search);
};

uint64_t DrmMemoryManager::getSystemSharedMemory() {
    uint64_t hostMemorySize = MemoryConstants::pageSize * (uint64_t)(sysconf(_SC_PHYS_PAGES));

    drm_i915_gem_get_aperture getAperture;
    auto ret = drm->ioctl(DRM_IOCTL_I915_GEM_GET_APERTURE, &getAperture);
    DEBUG_BREAK_IF(ret != 0);
    ((void)(ret));
    uint64_t gpuMemorySize = getAperture.aper_size;

    return std::min(hostMemorySize, gpuMemorySize);
}

uint64_t DrmMemoryManager::getMaxApplicationAddress() {
    return MemoryConstants::max32BitAppAddress + (uint64_t)is64bit * (MemoryConstants::max64BitAppAddress - MemoryConstants::max32BitAppAddress);
}

bool DrmMemoryManager::populateOsHandles(OsHandleStorage &handleStorage) {
    for (unsigned int i = 0; i < max_fragments_count; i++) {
        // If there is no fragment it means it already exists.
        if (!handleStorage.fragmentStorageData[i].osHandleStorage && handleStorage.fragmentStorageData[i].fragmentSize) {
            handleStorage.fragmentStorageData[i].osHandleStorage = new OsHandle();
            handleStorage.fragmentStorageData[i].residency = new ResidencyData();

            handleStorage.fragmentStorageData[i].osHandleStorage->bo = allocUserptr((uintptr_t)handleStorage.fragmentStorageData[i].cpuPtr,
                                                                                    handleStorage.fragmentStorageData[i].fragmentSize,
                                                                                    0,
                                                                                    true);
            if (!handleStorage.fragmentStorageData[i].osHandleStorage->bo) {
                handleStorage.fragmentStorageData[i].freeTheFragment = true;
                return false;
            }
            hostPtrManager.storeFragment(handleStorage.fragmentStorageData[i]);
        }
    }
    return true;
}
void DrmMemoryManager::cleanOsHandles(OsHandleStorage &handleStorage) {
    for (unsigned int i = 0; i < max_fragments_count; i++) {
        if (handleStorage.fragmentStorageData[i].freeTheFragment) {
            if (handleStorage.fragmentStorageData[i].osHandleStorage->bo) {
                BufferObject *search = handleStorage.fragmentStorageData[i].osHandleStorage->bo;
                search->wait(-1);
                auto refCount = unreference(search, true);
                DEBUG_BREAK_IF(refCount != 1u);
                ((void)(refCount));
            }
            delete handleStorage.fragmentStorageData[i].osHandleStorage;
            delete handleStorage.fragmentStorageData[i].residency;
        }
    }
}

BufferObject *DrmMemoryManager::getPinBB() const {
    return pinBB;
}
} // namespace OCLRT
