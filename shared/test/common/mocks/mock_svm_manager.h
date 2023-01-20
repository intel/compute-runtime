/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/fixtures/cpu_page_fault_manager_tests_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

namespace NEO {
struct MockSVMAllocsManager : public SVMAllocsManager {
  public:
    using SVMAllocsManager::memoryManager;
    using SVMAllocsManager::mtxForIndirectAccess;
    using SVMAllocsManager::multiOsContextSupport;
    using SVMAllocsManager::SVMAllocs;
    using SVMAllocsManager::SVMAllocsManager;
    using SVMAllocsManager::svmMapOperations;
    using SVMAllocsManager::usmDeviceAllocationsCache;
    using SVMAllocsManager::usmDeviceAllocationsCacheEnabled;
};

template <bool enableLocalMemory>
struct SVMMemoryAllocatorFixture {
    SVMMemoryAllocatorFixture() : executionEnvironment(defaultHwInfo.get()) {}

    void setUp() {
        bool svmSupported = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->capabilityTable.ftrSvm;
        if (!svmSupported) {
            GTEST_SKIP();
        }
        executionEnvironment.initGmm();
        memoryManager = std::make_unique<MockMemoryManager>(false, enableLocalMemory, executionEnvironment);
        svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get(), false);
        if (enableLocalMemory) {
            memoryManager->pageFaultManager.reset(new MockPageFaultManager);
        }
    }
    void tearDown() {
    }

    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<MockMemoryManager> memoryManager;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
};

} // namespace NEO
