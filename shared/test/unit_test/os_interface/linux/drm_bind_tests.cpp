/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper_with_capture.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

namespace NEO {
extern bool disableBindDefaultInTests;
}

TEST(DrmBindTest, givenBindAlreadyCompleteWhenWaitForBindThenWaitUserFenceIoctlIsNotCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();

    drm.pagingFence[0] = 31u;
    drm.fenceVal[0] = 31u;

    drm.waitForBind(0u);

    EXPECT_EQ(0u, drm.waitUserFenceParams.size());

    drm.pagingFence[0] = 49u;
    drm.fenceVal[0] = 31u;

    drm.waitForBind(0u);

    EXPECT_EQ(0u, drm.waitUserFenceParams.size());
}

TEST(DrmBindTest, givenBindNotCompleteWhenWaitForBindThenWaitUserFenceIoctlIsCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();

    drm.pagingFence[0] = 26u;
    drm.fenceVal[0] = 31u;

    drm.waitForBind(0u);

    EXPECT_EQ(1u, drm.waitUserFenceParams.size());
    EXPECT_EQ(0u, drm.waitUserFenceParams[0].ctxId);
    EXPECT_EQ(castToUint64(&drm.pagingFence[0]), drm.waitUserFenceParams[0].address);
    EXPECT_EQ(drm.ioctlHelper->getWaitUserFenceSoftFlag(), drm.waitUserFenceParams[0].flags);
    EXPECT_EQ(drm.fenceVal[0], drm.waitUserFenceParams[0].value);
    EXPECT_EQ(-1, drm.waitUserFenceParams[0].timeout);
}

TEST(DrmBindTest, whenCheckingVmBindAvailabilityThenIoctlHelperSupportIsUsed) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.callBaseIsVmBindAvailable = true;

    EXPECT_EQ(drm.isVmBindAvailable(), drm.getIoctlHelper()->isVmBindAvailable());
}

TEST(DrmBindTest, whenCheckingSetPairAvailabilityThenIoctlHelperSupportIsUsed) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.callBaseIsSetPairAvailable = true;

    EXPECT_EQ(drm.isSetPairAvailable(), drm.getIoctlHelper()->isSetPairAvailable());
}

TEST(DrmBindTest, whenCheckingChunkingAvailabilityThenIoctlHelperSupportIsUsed) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.callBaseIsChunkingAvailable = true;

    EXPECT_EQ(drm.isChunkingAvailable(), drm.getIoctlHelper()->isChunkingAvailable());
}

struct DrmBindCapabilityTest : public ::testing::Test {
    void SetUp() override {
        drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
        drm->callBaseIsVmBindAvailable = true;
        drm->callBaseIsSetPairAvailable = true;
        drm->callBaseIsChunkingAvailable = true;
        ioctlHelper = new MockIoctlHelperWithCapture(*drm);
        drm->ioctlHelper.reset(ioctlHelper);
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    std::unique_ptr<DrmMock> drm;
    MockIoctlHelperWithCapture *ioctlHelper = nullptr;
};

TEST_F(DrmBindCapabilityTest, givenHelperSupportingVmBindWhenQueryingVmBindAvailabilityTwiceThenHelperIsQueriedOnce) {
    VariableBackup<bool> disableBindBackup(&disableBindDefaultInTests, false);
    ioctlHelper->vmBindAvailable = true;

    EXPECT_TRUE(drm->isVmBindAvailable());
    EXPECT_TRUE(drm->isVmBindAvailable());
    EXPECT_TRUE(drm->bindAvailable);
    EXPECT_EQ(1u, ioctlHelper->isVmBindAvailableCalled);
}

TEST_F(DrmBindCapabilityTest, givenHelperNotSupportingVmBindWhenQueryingVmBindAvailabilityThenVmBindIsNotAvailable) {
    VariableBackup<bool> disableBindBackup(&disableBindDefaultInTests, false);
    ioctlHelper->vmBindAvailable = false;

    EXPECT_FALSE(drm->isVmBindAvailable());
    EXPECT_FALSE(drm->bindAvailable);
    EXPECT_EQ(1u, ioctlHelper->isVmBindAvailableCalled);
}

TEST_F(DrmBindCapabilityTest, givenUseVmBindSetAndHelperNotSupportingVmBindWhenQueryingVmBindAvailabilityThenSupportIsOverridden) {
    debugManager.flags.UseVmBind.set(1);
    ioctlHelper->vmBindAvailable = false;

    EXPECT_TRUE(drm->isVmBindAvailable());
    EXPECT_TRUE(drm->bindAvailable);
    EXPECT_EQ(1u, ioctlHelper->isVmBindAvailableCalled);
}

TEST_F(DrmBindCapabilityTest, givenEnableSetPairNotSetWhenQueryingSetPairAvailabilityThenHelperIsNotQueried) {
    for (auto enableSetPair : {-1, 0}) {
        DrmMock drmWithFlag{*executionEnvironment->rootDeviceEnvironments[0]};
        auto helper = new MockIoctlHelperWithCapture(drmWithFlag);
        drmWithFlag.ioctlHelper.reset(helper);
        drmWithFlag.callBaseIsSetPairAvailable = true;
        helper->setPairAvailable = true;
        debugManager.flags.EnableSetPair.set(enableSetPair);

        EXPECT_FALSE(drmWithFlag.isSetPairAvailable());
        EXPECT_FALSE(drmWithFlag.setPairAvailable);
        EXPECT_EQ(0u, helper->isSetPairAvailableCalled);
    }
}

TEST_F(DrmBindCapabilityTest, givenEnableSetPairSetWhenQueryingSetPairAvailabilityTwiceThenHelperResultIsReturnedAndQueriedOnce) {
    debugManager.flags.EnableSetPair.set(1);
    for (auto helperSupport : {true, false}) {
        DrmMock drmWithFlag{*executionEnvironment->rootDeviceEnvironments[0]};
        auto helper = new MockIoctlHelperWithCapture(drmWithFlag);
        drmWithFlag.ioctlHelper.reset(helper);
        drmWithFlag.callBaseIsSetPairAvailable = true;
        helper->setPairAvailable = helperSupport;

        EXPECT_EQ(helperSupport, drmWithFlag.isSetPairAvailable());
        EXPECT_EQ(helperSupport, drmWithFlag.isSetPairAvailable());
        EXPECT_EQ(helperSupport, drmWithFlag.setPairAvailable);
        EXPECT_EQ(1u, helper->isSetPairAvailableCalled);
    }
}

TEST_F(DrmBindCapabilityTest, givenEnableBOChunkingDisabledWhenQueryingChunkingAvailabilityThenHelperIsNotQueried) {
    debugManager.flags.EnableBOChunking.set(0);
    debugManager.flags.UseKmdMigration.set(1);
    ioctlHelper->chunkingAvailable = true;

    EXPECT_FALSE(drm->isChunkingAvailable());
    EXPECT_FALSE(drm->getChunkingAvailable());
    EXPECT_EQ(0u, drm->getChunkingMode());
    EXPECT_EQ(0u, ioctlHelper->isChunkingAvailableCalled);
}

TEST_F(DrmBindCapabilityTest, givenHelperNotSupportingChunkingWhenQueryingChunkingAvailabilityThenChunkingIsNotAvailable) {
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.UseKmdMigration.set(1);
    ioctlHelper->chunkingAvailable = false;

    EXPECT_FALSE(drm->isChunkingAvailable());
    EXPECT_FALSE(drm->chunkingAvailable);
    EXPECT_EQ(1u, ioctlHelper->isChunkingAvailableCalled);
}

TEST_F(DrmBindCapabilityTest, givenDefaultEnableBOChunkingAndHelperSupportingChunkingWhenQueryingChunkingAvailabilityTwiceThenDeviceChunkingIsAvailableAndHelperIsQueriedOnce) {
    for (auto useKmdMigration : {0, 1}) {
        DrmMock drmWithFlag{*executionEnvironment->rootDeviceEnvironments[0]};
        auto helper = new MockIoctlHelperWithCapture(drmWithFlag);
        drmWithFlag.ioctlHelper.reset(helper);
        drmWithFlag.callBaseIsChunkingAvailable = true;
        helper->chunkingAvailable = true;
        debugManager.flags.UseKmdMigration.set(useKmdMigration);

        EXPECT_TRUE(drmWithFlag.isChunkingAvailable());
        EXPECT_TRUE(drmWithFlag.isChunkingAvailable());
        EXPECT_TRUE(drmWithFlag.getChunkingAvailable());
        EXPECT_EQ(chunkingModeDevice, drmWithFlag.getChunkingMode());
        EXPECT_EQ(1u, helper->isChunkingAvailableCalled);
    }
}

TEST_F(DrmBindCapabilityTest, givenSharedChunkingRequestedWhenQueryingChunkingAvailabilityThenChunkingIsAvailableOnlyWithKmdMigration) {
    debugManager.flags.EnableBOChunking.set(chunkingModeShared);
    for (auto useKmdMigration : {0, 1}) {
        DrmMock drmWithFlag{*executionEnvironment->rootDeviceEnvironments[0]};
        auto helper = new MockIoctlHelperWithCapture(drmWithFlag);
        drmWithFlag.ioctlHelper.reset(helper);
        drmWithFlag.callBaseIsChunkingAvailable = true;
        helper->chunkingAvailable = true;
        debugManager.flags.UseKmdMigration.set(useKmdMigration);

        EXPECT_EQ(useKmdMigration == 1, drmWithFlag.isChunkingAvailable());
        EXPECT_EQ(1u, helper->isChunkingAvailableCalled);
    }
}

TEST_F(DrmBindCapabilityTest, givenMinimalAllocationSizeForChunkingSetWhenQueryingChunkingAvailabilityThenMinimalChunkingSizeIsOverridden) {
    const uint64_t minimalSizeForChunking = 64 * MemoryConstants::kiloByte;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.UseKmdMigration.set(1);
    debugManager.flags.MinimalAllocationSizeForChunking.set(minimalSizeForChunking);
    ioctlHelper->chunkingAvailable = true;

    EXPECT_TRUE(drm->isChunkingAvailable());
    EXPECT_EQ(minimalSizeForChunking, drm->minimalChunkingSize);
}
