/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_os_context_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

TEST(DrmVmBindTest, givenBoRequiringImmediateBindWhenBindingThenImmediateFlagIsPassed) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    MockBufferObject bo(0, &drm, 3, 0, 0, 1);
    bo.requireImmediateBinding(true);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    bo.bind(&osContext, 0, false);

    EXPECT_EQ(DrmPrelimHelper::getImmediateVmBindFlag(), drm.context.receivedVmBind->flags);
}

TEST(DrmVmBindTest, givenBoRequiringExplicitResidencyWhenBindingThenMakeResidentFlagIsPassedAndUserFenceIsSetup) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.pageFaultSupported = true;

    for (auto requireResidency : {false, true}) {
        MockBufferObject bo(0, &drm, 3, 0, 0, 1);
        bo.requireExplicitResidency(requireResidency);

        OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext.ensureContextInitialized(false);
        uint32_t vmHandleId = 0;
        bo.bind(&osContext, vmHandleId, false);

        if (requireResidency) {
            EXPECT_EQ(DrmPrelimHelper::getImmediateVmBindFlag() | DrmPrelimHelper::getMakeResidentVmBindFlag(), drm.context.receivedVmBind->flags);
            ASSERT_TRUE(drm.context.receivedVmBindUserFence);
            EXPECT_EQ(castToUint64(drm.getFenceAddr(vmHandleId)), drm.context.receivedVmBindUserFence->addr);
            EXPECT_EQ(drm.fenceVal[vmHandleId], drm.context.receivedVmBindUserFence->val);
        } else {
            EXPECT_EQ(DrmPrelimHelper::getImmediateVmBindFlag(), drm.context.receivedVmBind->flags);
        }
    }
}

TEST(DrmVmBindTest,
     givenBoWithChunkingRequiringExplicitResidencyWhenBindingThenMakeResidentFlagIsPassedAndUserFenceIsSetup) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.pageFaultSupported = true;

    for (auto requireResidency : {false, true}) {
        MockBufferObject bo(0, &drm, 3, 0, 0, 1);
        bo.setChunked(true);
        bo.requireExplicitResidency(requireResidency);

        OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext.ensureContextInitialized(false);
        uint32_t vmHandleId = 0;
        bo.bind(&osContext, vmHandleId, false);

        if (requireResidency) {
            EXPECT_EQ(DrmPrelimHelper::getImmediateVmBindFlag() | DrmPrelimHelper::getMakeResidentVmBindFlag(), drm.context.receivedVmBind->flags);
            ASSERT_TRUE(drm.context.receivedVmBindUserFence);
            EXPECT_EQ(castToUint64(drm.getFenceAddr(vmHandleId)), drm.context.receivedVmBindUserFence->addr);
            EXPECT_EQ(drm.fenceVal[vmHandleId], drm.context.receivedVmBindUserFence->val);
        } else {
            EXPECT_EQ(DrmPrelimHelper::getImmediateVmBindFlag(), drm.context.receivedVmBind->flags);
        }
    }
}

TEST(DrmVmBindTest, givenPerContextVmsAndBoRequiringExplicitResidencyWhenBindingThenPagingFenceFromContextIsUsed) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.pageFaultSupported = true;
    drm.requirePerContextVM = true;

    for (auto requireResidency : {false, true}) {
        MockBufferObject bo(0, &drm, 3, 0, 0, 1);
        bo.requireExplicitResidency(requireResidency);

        MockOsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext.ensureContextInitialized(false);
        uint32_t vmHandleId = 0;
        bo.bind(&osContext, vmHandleId, false);

        if (requireResidency) {
            EXPECT_EQ(DrmPrelimHelper::getImmediateVmBindFlag() | DrmPrelimHelper::getMakeResidentVmBindFlag(), drm.context.receivedVmBind->flags);
            ASSERT_TRUE(drm.context.receivedVmBindUserFence);
            EXPECT_EQ(castToUint64(osContext.getFenceAddr(vmHandleId)), drm.context.receivedVmBindUserFence->addr);
            EXPECT_EQ(osContext.fenceVal[vmHandleId], drm.context.receivedVmBindUserFence->val);
            EXPECT_EQ(1u, osContext.fenceVal[vmHandleId]);
        } else {
            EXPECT_EQ(DrmPrelimHelper::getImmediateVmBindFlag(), drm.context.receivedVmBind->flags);
        }
    }
}

TEST(DrmVmBindTest, whenCallingWaitForBindThenWaitUserFenceIsCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    struct DrmQueryMockToTestWaitForBind : public DrmQueryMock {
        using DrmQueryMock::DrmQueryMock;

        int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override {
            waitUserFenceCalled = true;
            return 0;
        }
        bool waitUserFenceCalled = false;
    };

    DrmQueryMockToTestWaitForBind drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.pageFaultSupported = true;

    for (auto requireResidency : {false, true}) {
        MockBufferObject bo(0, &drm, 3, 0, 0, 1);
        bo.requireExplicitResidency(requireResidency);

        OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext.ensureContextInitialized(false);
        uint32_t vmHandleId = 0;
        bo.bind(&osContext, vmHandleId, false);

        drm.waitForBind(vmHandleId);

        EXPECT_TRUE(drm.waitUserFenceCalled);
    }
}

TEST(DrmVmBindTest, givenUseKmdMigrationWhenCallingBindBoOnUnifiedSharedMemoryThenAllocationShouldPageFaultAndExplicitResidencyIsNotRequired) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.pageFaultSupported = true;

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    uint32_t vmHandleId = 0;

    MockBufferObject bo(0u, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::unifiedSharedMemory, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    allocation.bindBO(&bo, &osContext, vmHandleId, nullptr, true, false);

    EXPECT_TRUE(allocation.shouldAllocationPageFault(&drm));
    EXPECT_FALSE(bo.isExplicitResidencyRequired());
    EXPECT_EQ(DrmPrelimHelper::getImmediateVmBindFlag(), drm.context.receivedVmBind->flags);
}

TEST(DrmVmBindTest, givenDrmWithPageFaultSupportWhenCallingBindBoOnUnifiedSharedMemoryThenMarkAllocationShouldPageFaultWhenKmdMigrationIsSupported) {
    constexpr auto rootDeviceIndex{0U};
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->initGmm();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface.reset(new NEO::OSInterface);

    auto drm{new DrmQueryMock{*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]}};
    drm->pageFaultSupported = true;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>{drm});
    executionEnvironment->memoryManager.reset(new MockDrmMemoryManager{GemCloseWorkerMode::gemCloseWorkerInactive, false, false, *executionEnvironment});

    OsContextLinux osContext(*drm, rootDeviceIndex, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    uint32_t vmHandleId = 0;

    MockBufferObject bo(rootDeviceIndex, drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::unifiedSharedMemory, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    allocation.bindBO(&bo, &osContext, vmHandleId, nullptr, true, false);

    const bool isKmdMigrationAvailable{executionEnvironment->memoryManager->isKmdMigrationAvailable(rootDeviceIndex)};

    if (isKmdMigrationAvailable) {
        EXPECT_TRUE(allocation.shouldAllocationPageFault(drm));
        EXPECT_FALSE(bo.isExplicitResidencyRequired());
        EXPECT_EQ(DrmPrelimHelper::getImmediateVmBindFlag(), drm->context.receivedVmBind->flags);
    } else {
        EXPECT_FALSE(allocation.shouldAllocationPageFault(drm));
        EXPECT_TRUE(bo.isExplicitResidencyRequired());
        EXPECT_EQ(DrmPrelimHelper::getImmediateVmBindFlag() | DrmPrelimHelper::getMakeResidentVmBindFlag(), drm->context.receivedVmBind->flags);
    }
}
