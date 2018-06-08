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

#pragma once
#include <sys/ioctl.h>
#include <errno.h>
#include <stdint.h>
#include <cstdlib>

#include <atomic>
#include <set>
#include <vector>

struct drm_i915_gem_exec_object2;
struct drm_i915_gem_relocation_entry;

namespace OCLRT {

class DrmMemoryManager;
class Drm;

enum StorageAllocatorType {
    MMAP_ALLOCATOR,
    BIT32_ALLOCATOR_EXTERNAL,
    BIT32_ALLOCATOR_INTERNAL,
    MALLOC_ALLOCATOR,
    EXTERNAL_ALLOCATOR,
    UNKNOWN_ALLOCATOR
};

class BufferObject {
    friend DrmMemoryManager;
    using ResidencyVector = std::vector<BufferObject *>;

  public:
    MOCKABLE_VIRTUAL ~BufferObject(){};

    bool softPin(uint64_t offset);

    bool setTiling(uint32_t mode, uint32_t stride);

    MOCKABLE_VIRTUAL int pin(BufferObject *boToPin[], size_t numberOfBos);

    int exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency = false, bool lowPriority = false);

    int wait(int64_t timeoutNs);
    bool close();

    inline void reference() {
        this->refCount++;
    }
    uint32_t getRefCount() const;

    bool peekIsAllocated() const { return isAllocated; }
    size_t peekSize() const { return size; }
    int peekHandle() const { return handle; }
    void *peekAddress() const { return address; }
    void setAddress(void *address) { this->address = address; }
    void *peekLockedAddress() const { return lockedAddress; }
    void setLockedAddress(void *cpuAddress) { this->lockedAddress = cpuAddress; }
    void setUnmapSize(uint64_t unmapSize) { this->unmapSize = unmapSize; }
    uint64_t peekUnmapSize() const { return unmapSize; }
    void swapResidencyVector(ResidencyVector *residencyVect) {
        std::swap(this->residency, *residencyVect);
    }
    void setExecObjectsStorage(drm_i915_gem_exec_object2 *storage) {
        execObjectsStorage = storage;
    }
    ResidencyVector *getResidency() { return &residency; }
    StorageAllocatorType peekAllocationType() const { return storageAllocatorType; }
    void setAllocationType(StorageAllocatorType allocatorType) { this->storageAllocatorType = allocatorType; }
    bool peekIsResident() const { return isResident; }
    void setIsResident(bool isResident) { this->isResident = isResident; }

  protected:
    bool isResident;
    BufferObject(Drm *drm, int handle, bool isAllocated);

    Drm *drm;

    std::atomic<uint32_t> refCount;

    ResidencyVector residency;
    drm_i915_gem_exec_object2 *execObjectsStorage;

    int handle; // i915 gem object handle
    bool isSoftpin;
    bool isReused;

    //Tiling
    uint32_t tiling_mode;
    uint32_t stride;

    MOCKABLE_VIRTUAL void fillExecObject(drm_i915_gem_exec_object2 &execObject);
    void processRelocs(int &idx);

    uint64_t offset64; // last-seen GPU offset
    size_t size;
    void *address;       // GPU side virtual address
    void *lockedAddress; // CPU side virtual address

    bool isAllocated = false;
    uint64_t unmapSize = 0;
    StorageAllocatorType storageAllocatorType = UNKNOWN_ALLOCATOR;
};
} // namespace OCLRT
