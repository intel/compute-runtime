/*
* Copyright (c) 2018, Intel Corporation
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

#include "runtime/os_interface/linux/drm_memory_manager.h"

namespace OCLRT {
static off_t lseekReturn = 4096u;
static int lseekCalledCount = 0;
static int mmapMockCallCount = 0;
static int munmapMockCallCount = 0;

off_t lseekMock(int fd, off_t offset, int whence) noexcept {
    lseekCalledCount++;
    return lseekReturn;
}
void *mmapMock(void *addr, size_t length, int prot, int flags,
               int fd, long offset) noexcept {
    mmapMockCallCount++;
    return reinterpret_cast<void *>(0x1000);
}

int munmapMock(void *addr, size_t length) noexcept {
    munmapMockCallCount++;
    return 0;
}

int closeMock(int) {
    return 0;
}

class TestedDrmMemoryManager : public DrmMemoryManager {
  public:
    using DrmMemoryManager::allocUserptr;
    using DrmMemoryManager::setDomainCpu;
    using DrmMemoryManager::sharingBufferObjects;

    TestedDrmMemoryManager(Drm *drm) : DrmMemoryManager(drm, gemCloseWorkerMode::gemCloseWorkerInactive, false, false) {
        this->lseekFunction = &lseekMock;
        this->mmapFunction = &mmapMock;
        this->munmapFunction = &munmapMock;
        this->closeFunction = &closeMock;
        lseekReturn = 4096;
        lseekCalledCount = 0;
        mmapMockCallCount = 0;
        munmapMockCallCount = 0;
    };
    TestedDrmMemoryManager(Drm *drm, bool allowForcePin, bool validateHostPtrMemory) : DrmMemoryManager(drm, gemCloseWorkerMode::gemCloseWorkerInactive, allowForcePin, validateHostPtrMemory) {
        this->lseekFunction = &lseekMock;
        this->mmapFunction = &mmapMock;
        this->munmapFunction = &munmapMock;
        this->closeFunction = &closeMock;
        lseekReturn = 4096;
        lseekCalledCount = 0;
        mmapMockCallCount = 0;
        munmapMockCallCount = 0;
    }

    void unreference(BufferObject *bo) {
        DrmMemoryManager::unreference(bo);
    }

    void injectPinBB(BufferObject *newPinBB) {
        BufferObject *currentPinBB = pinBB;
        pinBB = nullptr;
        DrmMemoryManager::unreference(currentPinBB);
        pinBB = newPinBB;
    }

    DrmGemCloseWorker *getgemCloseWorker() { return this->gemCloseWorker.get(); }

    Allocator32bit *getDrmInternal32BitAllocator() { return internal32bitAllocator.get(); }
};
} // namespace OCLRT
