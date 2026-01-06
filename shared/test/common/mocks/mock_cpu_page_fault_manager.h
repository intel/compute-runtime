/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

using namespace NEO;

template <class BaseFaultManager>
class MockPageFaultManagerImpl : public BaseFaultManager {
  public:
    using BaseFaultManager::BaseFaultManager;
    using BaseFaultManager::gpuDomainHandler;
    using BaseFaultManager::memoryData;
    using PageFaultData = typename BaseFaultManager::PageFaultData;
    using BaseFaultManager::selectGpuDomainHandler;
    using BaseFaultManager::transferAndUnprotectMemory;
    using BaseFaultManager::unprotectAndTransferMemory;
    using BaseFaultManager::verifyAndHandlePageFault;

    bool checkFaultHandlerFromPageFaultManager() override {
        checkFaultHandlerCalled++;
        return isFaultHandlerFromPageFaultManager;
    }
    void registerFaultHandler() override {
        registerFaultHandlerCalled++;
    }
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
    void protectCpuMemoryFromWrites(void *ptr, size_t size) override {
        protectWritesCalled++;
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
    void setCpuAllocEvictable(bool evictable, void *ptr, SVMAllocsManager *unifiedMemoryManager) override {
        setCpuAllocEvictableCalled++;
        isCpuAllocEvictable = evictable;
    }
    void allowCPUMemoryEviction(bool evict, void *ptr, PageFaultData &pageFaultData) override {
        allowCPUMemoryEvictionCalled++;
    }
    void baseAubWritable(bool writable, void *ptr, SVMAllocsManager *unifiedMemoryManager) {
        BaseFaultManager::setAubWritable(writable, ptr, unifiedMemoryManager);
    }
    void baseCpuTransfer(void *ptr, size_t size, void *cmdQ) {
        BaseFaultManager::transferToCpu(ptr, size, cmdQ);
    }
    void baseGpuTransfer(void *ptr, void *cmdQ) {
        BaseFaultManager::transferToGpu(ptr, cmdQ);
    }
    void baseCpuAllocEvictable(bool evictable, void *ptr, SVMAllocsManager *unifiedMemoryManager) {
        BaseFaultManager::setCpuAllocEvictable(evictable, ptr, unifiedMemoryManager);
    }
    void baseAllowCPUMemoryEviction(bool evict, void *ptr, PageFaultData &pageFaultData) {
        BaseFaultManager::allowCPUMemoryEviction(evict, ptr, pageFaultData);
    }

    void allowCPUMemoryEvictionImpl(bool evict, void *ptr, CommandStreamReceiver &csr, OSInterface *osInterface) override {
        allowCPUMemoryEvictionImplCalled++;
        engineType = csr.getOsContext().getEngineType();
        engineUsage = csr.getOsContext().getEngineUsage();
    }

    void *getHwHandlerAddress() {
        return reinterpret_cast<void *>(BaseFaultManager::transferAndUnprotectMemory);
    }

    void *getAubAndTbxHandlerAddress() {
        return reinterpret_cast<void *>(BaseFaultManager::unprotectAndTransferMemory);
    }
    void moveAllocationToGpuDomain(void *ptr) override {
        moveAllocationToGpuDomainCalled++;
        BaseFaultManager::moveAllocationToGpuDomain(ptr);
    }

    int checkFaultHandlerCalled = 0;
    int registerFaultHandlerCalled = 0;
    int allowMemoryAccessCalled = 0;
    int protectMemoryCalled = 0;
    int protectWritesCalled = 0;
    int transferToCpuCalled = 0;
    int transferToGpuCalled = 0;
    int moveAllocationToGpuDomainCalled = 0;
    int setCpuAllocEvictableCalled = 0;
    int allowCPUMemoryEvictionCalled = 0;
    int allowCPUMemoryEvictionImplCalled = 0;
    void *transferToCpuAddress = nullptr;
    void *transferToGpuAddress = nullptr;
    void *allowedMemoryAccessAddress = nullptr;
    void *protectedMemoryAccessAddress = nullptr;
    size_t transferToCpuSize = 0;
    size_t accessAllowedSize = 0;
    size_t protectedSize = 0;
    bool isAubWritable = true;
    bool isCpuAllocEvictable = true;
    bool isFaultHandlerFromPageFaultManager = false;
    aub_stream::EngineType engineType = aub_stream::EngineType::NUM_ENGINES;
    EngineUsage engineUsage = EngineUsage::engineUsageCount;
};

class MockPageFaultManager : public MockPageFaultManagerImpl<CpuPageFaultManager> {};

template <class T>
class MockPageFaultManagerHandlerInvoke : public T {
  public:
    using T::allowCPUMemoryAccess;
    using T::allowCPUMemoryEvictionImpl;
    using T::checkFaultHandlerFromPageFaultManager;
    using T::protectCPUMemoryAccess;
    using T::registerFaultHandler;
    using T::T;

    bool verifyAndHandlePageFault(void *ptr, bool handlePageFault) override {
        handlerInvoked = handlePageFault;
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
