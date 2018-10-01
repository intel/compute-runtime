/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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

    TestedDrmMemoryManager(Drm *drm, ExecutionEnvironment &executionEnvironment) : DrmMemoryManager(drm, gemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {
        this->lseekFunction = &lseekMock;
        this->mmapFunction = &mmapMock;
        this->munmapFunction = &munmapMock;
        this->closeFunction = &closeMock;
        lseekReturn = 4096;
        lseekCalledCount = 0;
        mmapMockCallCount = 0;
        munmapMockCallCount = 0;
    };
    TestedDrmMemoryManager(Drm *drm, bool allowForcePin, bool validateHostPtrMemory, ExecutionEnvironment &executionEnvironment) : DrmMemoryManager(drm, gemCloseWorkerMode::gemCloseWorkerInactive, allowForcePin, validateHostPtrMemory, executionEnvironment) {
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
