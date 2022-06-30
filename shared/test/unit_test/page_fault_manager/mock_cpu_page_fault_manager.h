/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

using namespace NEO;

class MockPageFaultManager : public PageFaultManager {
  public:
    using PageFaultManager::gpuDomainHandler;
    using PageFaultManager::handleGpuDomainTransferForAubAndTbx;
    using PageFaultManager::handleGpuDomainTransferForHw;
    using PageFaultManager::memoryData;
    using PageFaultManager::PageFaultData;
    using PageFaultManager::PageFaultManager;
    using PageFaultManager::selectGpuDomainHandler;
    using PageFaultManager::verifyPageFault;

    void allowCPUMemoryAccess(void *ptr, size_t size) override {
        allowMemoryAccessCalled++;
        allowedMemoryAccessAddress = ptr;
        accessAllowedSize = size;
    }
    void protectCPUMemoryAccess(void *ptr, size_t size) override {
        protectMemoryCalled++;
        protectedMemoryAccessAddress = ptr;
        protectedSize = size;
    }
    void transferToCpu(void *ptr, size_t size, void *cmdQ) override {
        transferToCpuCalled++;
        transferToCpuAddress = ptr;
        transferToCpuSize = size;
    }
    void transferToGpu(void *ptr, void *cmdQ) override {
        transferToGpuCalled++;
        transferToGpuAddress = ptr;
    }
    void setAubWritable(bool writable, void *ptr, SVMAllocsManager *unifiedMemoryManager) override {
        isAubWritable = writable;
    }
    void baseAubWritable(bool writable, void *ptr, SVMAllocsManager *unifiedMemoryManager) {
        PageFaultManager::setAubWritable(writable, ptr, unifiedMemoryManager);
    }
    void baseCpuTransfer(void *ptr, size_t size, void *cmdQ) {
        PageFaultManager::transferToCpu(ptr, size, cmdQ);
    }
    void baseGpuTransfer(void *ptr, void *cmdQ) {
        PageFaultManager::transferToGpu(ptr, cmdQ);
    }
    void evictMemoryAfterImplCopy(GraphicsAllocation *allocation, Device *device) override {}

    void *getHwHandlerAddress() {
        return reinterpret_cast<void *>(PageFaultManager::handleGpuDomainTransferForHw);
    }

    void *getAubAndTbxHandlerAddress() {
        return reinterpret_cast<void *>(PageFaultManager::handleGpuDomainTransferForAubAndTbx);
    }
    void moveAllocationToGpuDomain(void *ptr) override {
        moveAllocationToGpuDomainCalled++;
        PageFaultManager::moveAllocationToGpuDomain(ptr);
    }

    int allowMemoryAccessCalled = 0;
    int protectMemoryCalled = 0;
    int transferToCpuCalled = 0;
    int transferToGpuCalled = 0;
    int moveAllocationToGpuDomainCalled = 0;
    void *transferToCpuAddress = nullptr;
    void *transferToGpuAddress = nullptr;
    void *allowedMemoryAccessAddress = nullptr;
    void *protectedMemoryAccessAddress = nullptr;
    size_t transferToCpuSize = 0;
    size_t accessAllowedSize = 0;
    size_t protectedSize = 0;
    bool isAubWritable = true;
};

template <class T>
class MockPageFaultManagerHandlerInvoke : public T {
  public:
    using T::allowCPUMemoryAccess;
    using T::evictMemoryAfterImplCopy;
    using T::protectCPUMemoryAccess;
    using T::T;

    bool verifyPageFault(void *ptr) override {
        handlerInvoked = true;
        if (allowCPUMemoryAccessOnPageFault) {
            this->allowCPUMemoryAccess(ptr, size);
        }
        return returnStatus;
    }

    bool returnStatus = true;
    bool allowCPUMemoryAccessOnPageFault = false;
    bool handlerInvoked = false;
    size_t size = 65536;
};
