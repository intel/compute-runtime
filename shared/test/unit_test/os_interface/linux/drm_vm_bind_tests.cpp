/*
 * Copyright (C) 2018-2026 Intel Corporation
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
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper_with_capture.h"
#include "shared/test/common/mocks/linux/mock_os_context_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

TEST(DrmVmBindTest, givenBoRequiringImmediateBindWhenBindingThenImmediateFlagIsPassed) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;

    MockBufferObject bo(0, &drm, 3, 0, 0, 1);
    bo.requireImmediateBinding(true);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    bo.bind(&osContext, 0, false);

    EXPECT_EQ(MockIoctlHelperWithCapture::vmBindFlagImmediate, drm.getMockIoctlHelper()->receivedVmBind->flags);
}

TEST(DrmVmBindTest, givenBoRequiringExplicitResidencyWhenBindingThenMakeResidentFlagIsPassedAndUserFenceIsSetup) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    drm.pageFaultSupported = true;

    for (auto requireResidency : {false, true}) {
        MockBufferObject bo(0, &drm, 3, 0, 0, 1);
        bo.requireExplicitResidency(requireResidency);

        OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext.ensureContextInitialized();
        uint32_t vmHandleId = 0;
        bo.bind(&osContext, vmHandleId, false);

        if (requireResidency) {
            EXPECT_EQ(MockIoctlHelperWithCapture::vmBindFlagImmediate | MockIoctlHelperWithCapture::vmBindFlagMakeResident, drm.getMockIoctlHelper()->receivedVmBind->flags);
            ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmBindUserFence);
            EXPECT_EQ(castToUint64(drm.getFenceAddr(vmHandleId)), drm.getMockIoctlHelper()->receivedVmBindUserFence->address);
            EXPECT_EQ(drm.fenceVal[vmHandleId], drm.getMockIoctlHelper()->receivedVmBindUserFence->value);
        } else {
            EXPECT_EQ(MockIoctlHelperWithCapture::vmBindFlagImmediate, drm.getMockIoctlHelper()->receivedVmBind->flags);
        }
    }
}

TEST(DrmVmBindTest,
     givenBoWithChunkingRequiringExplicitResidencyWhenBindingThenMakeResidentFlagIsPassedAndUserFenceIsSetup) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    drm.pageFaultSupported = true;

    for (auto requireResidency : {false, true}) {
        MockBufferObject bo(0, &drm, 3, 0, 0, 1);
        bo.setChunked(true);
        bo.requireExplicitResidency(requireResidency);

        OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext.ensureContextInitialized();
        uint32_t vmHandleId = 0;
        bo.bind(&osContext, vmHandleId, false);

        if (requireResidency) {
            EXPECT_EQ(MockIoctlHelperWithCapture::vmBindFlagImmediate | MockIoctlHelperWithCapture::vmBindFlagMakeResident, drm.getMockIoctlHelper()->receivedVmBind->flags);
            ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmBindUserFence);
            EXPECT_EQ(castToUint64(drm.getFenceAddr(vmHandleId)), drm.getMockIoctlHelper()->receivedVmBindUserFence->address);
            EXPECT_EQ(drm.fenceVal[vmHandleId], drm.getMockIoctlHelper()->receivedVmBindUserFence->value);
        } else {
            EXPECT_EQ(MockIoctlHelperWithCapture::vmBindFlagImmediate, drm.getMockIoctlHelper()->receivedVmBind->flags);
        }
    }
}

TEST(DrmVmBindTest, givenPerContextVmsAndBoRequiringExplicitResidencyWhenBindingThenPagingFenceFromContextIsUsed) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    drm.pageFaultSupported = true;
    drm.requirePerContextVM = true;

    for (auto requireResidency : {false, true}) {
        MockBufferObject bo(0, &drm, 3, 0, 0, 1);
        bo.requireExplicitResidency(requireResidency);

        MockOsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext.ensureContextInitialized();
        uint32_t vmHandleId = 0;
        bo.bind(&osContext, vmHandleId, false);

        if (requireResidency) {
            EXPECT_EQ(MockIoctlHelperWithCapture::vmBindFlagImmediate | MockIoctlHelperWithCapture::vmBindFlagMakeResident, drm.getMockIoctlHelper()->receivedVmBind->flags);
            ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmBindUserFence);
            EXPECT_EQ(castToUint64(osContext.getFenceAddr(vmHandleId)), drm.getMockIoctlHelper()->receivedVmBindUserFence->address);
            EXPECT_EQ(osContext.fenceVal[vmHandleId], drm.getMockIoctlHelper()->receivedVmBindUserFence->value);
            EXPECT_EQ(1u, osContext.fenceVal[vmHandleId]);
        } else {
            EXPECT_EQ(MockIoctlHelperWithCapture::vmBindFlagImmediate, drm.getMockIoctlHelper()->receivedVmBind->flags);
        }
    }
}

TEST(DrmVmBindTest, whenCallingWaitForBindThenWaitUserFenceIsCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    struct DrmMockToTestWaitForBind : public DrmMockWithCaptureHelper {
        using DrmMockWithCaptureHelper::DrmMockWithCaptureHelper;

        int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override {
            waitUserFenceCalled = true;
            return 0;
        }
        bool waitUserFenceCalled = false;
    };

    DrmMockToTestWaitForBind drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    drm.pageFaultSupported = true;

    for (auto requireResidency : {false, true}) {
        MockBufferObject bo(0, &drm, 3, 0, 0, 1);
        bo.requireExplicitResidency(requireResidency);

        OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext.ensureContextInitialized();
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

    DrmMockWithCaptureHelper drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    drm.pageFaultSupported = true;

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    uint32_t vmHandleId = 0;

    MockBufferObject bo(0u, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::unifiedSharedMemory, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    allocation.bindBO(&bo, &osContext, vmHandleId, nullptr, true, false);

    EXPECT_TRUE(allocation.shouldAllocationPageFault(&drm));
    EXPECT_FALSE(bo.isExplicitResidencyRequired());
    EXPECT_EQ(MockIoctlHelperWithCapture::vmBindFlagImmediate, drm.getMockIoctlHelper()->receivedVmBind->flags);
}

TEST(DrmVmBindTest, givenDrmWithPageFaultSupportWhenCallingBindBoOnUnifiedSharedMemoryThenMarkAllocationShouldPageFaultWhenKmdMigrationIsSupported) {
    constexpr auto rootDeviceIndex{0U};
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->initGmm();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface.reset(new NEO::OSInterface);

    auto drm{new DrmMockWithCaptureHelper{*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]}};
    drm->getMockIoctlHelper()->userFenceSetupRequired = true;
    drm->pageFaultSupported = true;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>{drm});
    executionEnvironment->memoryManager.reset(new MockDrmMemoryManager{GemCloseWorkerMode::gemCloseWorkerInactive, false, false, *executionEnvironment});

    OsContextLinux osContext(*drm, rootDeviceIndex, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    uint32_t vmHandleId = 0;

    MockBufferObject bo(rootDeviceIndex, drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::unifiedSharedMemory, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    allocation.bindBO(&bo, &osContext, vmHandleId, nullptr, true, false);

    const bool isKmdMigrationAvailable{executionEnvironment->memoryManager->isKmdMigrationAvailable(rootDeviceIndex)};

    if (isKmdMigrationAvailable) {
        EXPECT_TRUE(allocation.shouldAllocationPageFault(drm));
        EXPECT_FALSE(bo.isExplicitResidencyRequired());
        EXPECT_EQ(MockIoctlHelperWithCapture::vmBindFlagImmediate, drm->getMockIoctlHelper()->receivedVmBind->flags);
    } else {
        EXPECT_FALSE(allocation.shouldAllocationPageFault(drm));
        EXPECT_TRUE(bo.isExplicitResidencyRequired());
        EXPECT_EQ(MockIoctlHelperWithCapture::vmBindFlagImmediate | MockIoctlHelperWithCapture::vmBindFlagMakeResident, drm->getMockIoctlHelper()->receivedVmBind->flags);
    }
}

TEST(DrmVmBindTest, givenAsyncPagingFenceRequiredWhenBindingThenAsyncPagingFenceAddressIsUsedForUserFence) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    drm.pageFaultSupported = true;

    MockBufferObject bo(0, &drm, 3, 0, 0, 1);
    bo.requireExplicitResidency(true);
    bo.requireImmediateBinding(true);
    bo.setAsyncPagingFenceRequired();

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    uint32_t vmHandleId = 0;
    bo.bind(&osContext, vmHandleId, false);
    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmBindUserFence);

    EXPECT_EQ(bo.getAsyncFenceVal(&osContext, vmHandleId), drm.getMockIoctlHelper()->receivedVmBindUserFence->value);
    EXPECT_EQ(castToUint64(bo.getAsyncFenceAddr(&osContext, vmHandleId)), drm.getMockIoctlHelper()->receivedVmBindUserFence->address);
    EXPECT_NE(castToUint64(drm.getFenceAddr(vmHandleId)), drm.getMockIoctlHelper()->receivedVmBindUserFence->address);
}

TEST(DrmVmBindTest, givenAsyncPagingFenceRequiredWhenBindingThenAsyncFenceValueIsIncremented) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    drm.pageFaultSupported = true;

    MockBufferObject bo(0, &drm, 3, 0, 0, 1);
    bo.requireExplicitResidency(true);
    bo.requireImmediateBinding(true);
    bo.setAsyncPagingFenceRequired();

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    uint32_t vmHandleId = 0;

    auto asyncFenceValBefore = bo.getAsyncFenceVal(&osContext, vmHandleId);
    auto regularFenceValBefore = drm.getNextFenceVal(vmHandleId) - 1;

    bo.bind(&osContext, vmHandleId, false);

    auto asyncFenceValAfter = bo.getAsyncFenceVal(&osContext, vmHandleId);
    auto regularFenceValAfter = drm.getNextFenceVal(vmHandleId) - 1;
    EXPECT_GT(asyncFenceValAfter, asyncFenceValBefore);
    EXPECT_EQ(regularFenceValAfter, regularFenceValBefore);
}

TEST(DrmVmBindTest, givenAsyncPagingFenceRequiredAndPerContextVmsWhenBindingThenAsyncPagingFenceFromContextIsUsed) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    drm.pageFaultSupported = true;
    drm.requirePerContextVM = true;

    MockBufferObject bo(0, &drm, 3, 0, 0, 1);
    bo.requireExplicitResidency(true);
    bo.requireImmediateBinding(true);
    bo.setAsyncPagingFenceRequired();

    MockOsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    uint32_t vmHandleId = 0;

    auto asyncFenceValBefore = bo.getAsyncFenceVal(&osContext, vmHandleId);
    auto regularFenceValBefore = osContext.getNextFenceVal(vmHandleId) - 1;

    bo.bind(&osContext, vmHandleId, false);

    auto asyncFenceValAfter = bo.getAsyncFenceVal(&osContext, vmHandleId);
    auto regularFenceValAfter = osContext.getNextFenceVal(vmHandleId) - 1;

    EXPECT_GT(asyncFenceValAfter, asyncFenceValBefore);
    EXPECT_EQ(regularFenceValAfter, regularFenceValBefore);
    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmBindUserFence);
    EXPECT_EQ(castToUint64(bo.getAsyncFenceAddr(&osContext, vmHandleId)), drm.getMockIoctlHelper()->receivedVmBindUserFence->address);
    EXPECT_NE(castToUint64(osContext.getFenceAddr(vmHandleId)), drm.getMockIoctlHelper()->receivedVmBindUserFence->address);
}

TEST(DrmVmBindTest, givenAsyncPagingFenceRequiredWhenBindingThenBindFenceMutexIsNotAcquired) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    drm.pageFaultSupported = true;

    MockBufferObject bo(0, &drm, 3, 0, 0, 1);
    bo.requireExplicitResidency(true);
    bo.requireImmediateBinding(true);
    bo.setAsyncPagingFenceRequired();

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    uint32_t vmHandleId = 0;

    bo.bind(&osContext, vmHandleId, false);

    EXPECT_EQ(0u, drm.lockBindFenceMutexCallCount);
}

TEST(DrmVmBindTest, givenAsyncPagingFenceRequiredWhenUnbindingThenRegularPagingFenceIsUsed) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    drm.pageFaultSupported = true;

    MockBufferObject bo(0, &drm, 3, 0, 0, 1);
    bo.requireExplicitResidency(true);
    bo.requireImmediateBinding(true);
    bo.setAsyncPagingFenceRequired();

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    uint32_t vmHandleId = 0;

    bo.bind(&osContext, vmHandleId, false);

    drm.getMockIoctlHelper()->receivedVmBindUserFence.reset();

    bo.unbind(&osContext, vmHandleId);

    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmBindUserFence);
    EXPECT_EQ(castToUint64(drm.getFenceAddr(vmHandleId)), drm.getMockIoctlHelper()->receivedVmBindUserFence->address);
}

TEST(DrmVmBindTest, givenBoWithNonZeroVirtualMappingSizeWhenBindingThenVirtualMappingSizeIsUsedForLength) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;

    // Size != virtualMappingSize so we can verify which one is used
    MockBufferObject bo(0, &drm, 3, MemoryConstants::pageSize, 0, 1);
    bo.setVirtualMappingSize(MemoryConstants::pageSize64k);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    bo.bind(&osContext, 0, false);

    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmBind);
    EXPECT_EQ(MemoryConstants::pageSize64k, drm.getMockIoctlHelper()->receivedVmBind->length);
}

TEST(DrmVmBindTest, givenBufferObjectSetToColourWithBindWhenBindingThenSetProperAddressAndSize) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    MockBufferObject bo(0u, &drm, 3, 1, 0, 1);
    bo.setColourWithBind();
    bo.setColourChunk(MemoryConstants::pageSize64k);
    bo.addColouringAddress(0xffeeffee);
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();

    bo.bind(&osContext, 0, false);
    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmBind);
    EXPECT_EQ(drm.getMockIoctlHelper()->receivedVmBind->length, MemoryConstants::pageSize64k);
    EXPECT_EQ(drm.getMockIoctlHelper()->receivedVmBind->start, 0xffeeffee);
}

TEST(DrmVmBindTest, givenBufferObjectMarkedForCaptureAndDebuggerEnabledWhenBindingThenCaptureFlagIsSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    MockBufferObject bo(0, &drm, 3, 1, 0, 1);
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    bo.markForCapture();

    bo.bind(&osContext, 0, false);
    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmBind);
    EXPECT_TRUE(drm.getMockIoctlHelper()->receivedVmBind->flags & MockIoctlHelperWithCapture::vmBindFlagCapture);
}

TEST(DrmVmBindTest, givenBufferObjectMarkedForCaptureAndDebuggerNotEnabledWhenBindingThenCaptureFlagIsNotSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    MockBufferObject bo(0, &drm, 3, 1, 0, 1);
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    bo.markForCapture();

    bo.bind(&osContext, 0, false);
    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmBind);
    EXPECT_FALSE(drm.getMockIoctlHelper()->receivedVmBind->flags & MockIoctlHelperWithCapture::vmBindFlagCapture);
}

TEST(DrmVmBindTest, givenNoActiveDirectSubmissionAndForceUseImmediateExtensionWhenBindingThenImmediateFlagIsSetAndExtensionListIsNotNull) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableImmediateVmBindExt.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    MockBufferObject bo(0u, &drm, 3, 1, 0, 1);
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();

    bo.bind(&osContext, 0, false);
    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmBind);
    EXPECT_TRUE(drm.getMockIoctlHelper()->receivedVmBind->flags & MockIoctlHelperWithCapture::vmBindFlagImmediate);
    EXPECT_TRUE(drm.getMockIoctlHelper()->receivedVmBindUserFence);
}

TEST(DrmVmBindTest, whenBindingThenImmediateFlagIsSetAndExtensionListIsNotNull) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.getMockIoctlHelper()->userFenceSetupRequired = true;
    drm.setDirectSubmissionActive(true);
    MockBufferObject bo(0u, &drm, 3, 1, 0, 1);
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    osContext.setDirectSubmissionActive();

    bo.bind(&osContext, 0, false);
    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmBind);
    EXPECT_TRUE(drm.getMockIoctlHelper()->receivedVmBind->flags & MockIoctlHelperWithCapture::vmBindFlagImmediate);
    EXPECT_TRUE(drm.getMockIoctlHelper()->receivedVmBindUserFence);
}

TEST(DrmVmBindTest, givenBindExtHandlesWhenBindingWithinDefaultEngineContextThenBindExtensionsArePreparedWithHandles) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};

    MockBufferObject bo(0, &drm, 3, 0, 0, 1);
    bo.addBindExtHandle(4);
    bo.addBindExtHandle(5);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    bo.bind(&osContext, 0, false);

    auto ioctlHelper = drm.getMockIoctlHelper();
    ASSERT_EQ(1u, ioctlHelper->prepareVmBindExtCalls.size());
    EXPECT_EQ((std::vector<uint32_t>{4u, 5u}), ioctlHelper->prepareVmBindExtCalls[0]);
    ASSERT_TRUE(ioctlHelper->receivedVmBind);
    EXPECT_NE(0u, ioctlHelper->receivedVmBind->extensions);
}

TEST(DrmVmBindTest, givenBindExtHandlesWhenBindingWithinInternalOrCopyEngineContextThenBindExtensionsAreNotPrepared) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};

    for (auto engineTypeUsage : {EngineTypeUsage{aub_stream::EngineType::ENGINE_RCS, EngineUsage::internal},
                                 EngineTypeUsage{aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular}}) {
        MockBufferObject bo(0, &drm, 3, 0, 0, 1);
        bo.addBindExtHandle(4);
        bo.addBindExtHandle(5);

        OsContextLinux osContext(drm, 0, 0u, {engineTypeUsage, 1 /*deviceBitfield*/, PreemptionMode::Disabled, true /* isRootDevice*/});
        osContext.ensureContextInitialized();
        bo.bind(&osContext, 0, false);
    }

    EXPECT_EQ(2u, drm.getMockIoctlHelper()->vmBindCalled);
    EXPECT_TRUE(drm.getMockIoctlHelper()->prepareVmBindExtCalls.empty());
}

TEST(DrmVmBindTest, givenBindExtHandlesWhenUnbindingThenBindExtensionsAreNotPrepared) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};

    MockBufferObject bo(0, &drm, 3, 0, 0, 1);
    bo.addBindExtHandle(4);
    bo.addBindExtHandle(5);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    bo.bind(&osContext, 0, false);
    bo.unbind(&osContext, 0);

    EXPECT_EQ(1u, drm.getMockIoctlHelper()->vmUnbindCalled);
    EXPECT_EQ(1u, drm.getMockIoctlHelper()->prepareVmBindExtCalls.size());
}
