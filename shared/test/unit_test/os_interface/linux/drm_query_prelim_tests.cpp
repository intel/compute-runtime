/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock_prelim_context.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmQueryTest, givenDirectSubmissionActiveWhenCreateDrmContextThenProperFlagIsSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.createDrmContext(0, true, false);

    EXPECT_TRUE(drm.receivedContextCreateFlags & DrmPrelimHelper::getLongRunningContextCreateFlag());
}

TEST(DrmQueryTest, givenDirectSubmissionDisabledAndDirectSubmissionDrmContextSetWhenCreateDrmContextThenProperFlagIsSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionDrmContext.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.createDrmContext(0, false, false);

    EXPECT_TRUE(drm.receivedContextCreateFlags & DrmPrelimHelper::getLongRunningContextCreateFlag());
}

TEST(DrmQueryTest, givenDirectSubmissionActiveAndDirectSubmissionDrmContextSetZeroWhenCreateDrmContextThenProperFlagIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionDrmContext.set(0);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.createDrmContext(0, true, false);

    EXPECT_FALSE(drm.receivedContextCreateFlags & DrmPrelimHelper::getLongRunningContextCreateFlag());
}

TEST(DrmQueryTest, givenCooperativeEngineWhenCreateDrmContextThenRunAloneContextIsRequested) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.createDrmContext(0, false, true);

    const auto &extSetparam = drm.receivedContextCreateSetParam;

    EXPECT_EQ(static_cast<uint32_t>(I915_CONTEXT_CREATE_EXT_SETPARAM), extSetparam.base.name);
    EXPECT_EQ(0u, extSetparam.base.nextExtension);
    EXPECT_EQ(0u, extSetparam.base.flags);

    EXPECT_EQ(static_cast<uint64_t>(DrmPrelimHelper::getRunAloneContextParam()), extSetparam.param.param);
    EXPECT_EQ(0u, extSetparam.param.size);
    EXPECT_EQ(0u, extSetparam.param.contextId);
    EXPECT_EQ(0u, extSetparam.param.value);
}

TEST(DrmQueryTest, givenForceRunAloneContextFlagSetWhenCreateDrmContextThenRunAloneContextIsRequested) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    {
        debugManager.flags.ForceRunAloneContext.set(0);
        drm.createDrmContext(0, false, false);

        auto extSetparam = drm.receivedContextCreateSetParam;
        EXPECT_NE(static_cast<uint64_t>(DrmPrelimHelper::getRunAloneContextParam()), extSetparam.param.param);
    }
    {
        debugManager.flags.ForceRunAloneContext.set(1);
        drm.createDrmContext(0, false, false);

        auto extSetparam = drm.receivedContextCreateSetParam;
        EXPECT_EQ(static_cast<uint32_t>(I915_CONTEXT_CREATE_EXT_SETPARAM), extSetparam.base.name);
        EXPECT_EQ(0u, extSetparam.base.nextExtension);
        EXPECT_EQ(0u, extSetparam.base.flags);

        EXPECT_EQ(static_cast<uint64_t>(DrmPrelimHelper::getRunAloneContextParam()), extSetparam.param.param);
        EXPECT_EQ(0u, extSetparam.param.size);
        EXPECT_EQ(0u, extSetparam.param.contextId);
        EXPECT_EQ(0u, extSetparam.param.value);
    }
}

TEST(DrmQueryTest, givenCreateContextWithAccessCountersWhenDrmContextIsCreatedThenProgramAccessCountersWithDefaultGranularity) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateContextWithAccessCounters.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto ret = drm.createDrmContext(0, false, false);
    EXPECT_EQ(0, ret);

    EXPECT_TRUE(drm.receivedContextCreateFlags & I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS);

    auto extSetparam = drm.receivedContextCreateSetParam;

    EXPECT_EQ(static_cast<uint32_t>(I915_CONTEXT_CREATE_EXT_SETPARAM), extSetparam.base.name);
    EXPECT_EQ(0u, extSetparam.base.nextExtension);
    EXPECT_EQ(0u, extSetparam.base.flags);

    EXPECT_EQ(static_cast<uint64_t>(DrmPrelimHelper::getAccContextParam()), extSetparam.param.param);
    EXPECT_EQ(DrmPrelimHelper::getAccContextParamSize(), extSetparam.param.size);
    EXPECT_EQ(0u, extSetparam.param.contextId);
    EXPECT_NE(0u, extSetparam.param.value);

    auto paramAcc = drm.context.receivedContextParamAcc;
    ASSERT_TRUE(paramAcc);
    EXPECT_EQ(1u, paramAcc->notify);
    EXPECT_EQ(0u, paramAcc->trigger);
    EXPECT_EQ(static_cast<uint8_t>(DrmPrelimHelper::getContextAcgValues()[1]), paramAcc->granularity);
}

TEST(DrmQueryTest, GivenDrmWhenAskedForContextThatFailsThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    pDrm->storedRetVal = -1;
    EXPECT_EQ(-1, pDrm->createDrmContext(1, false, false));
    pDrm->storedRetVal = 0;
    delete pDrm;
}

TEST(DrmQueryTest, givenCreateContextWithAccessCounterWhenDrmContextIsCreatedThenProgramAccessCountersWithSpecifiedTriggeringThreshold) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateContextWithAccessCounters.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    for (uint16_t threshold : {0, 1, 1024, 65535}) {
        debugManager.flags.AccessCountersTrigger.set(threshold);

        auto ret = drm.createDrmContext(0, false, false);
        EXPECT_EQ(0, ret);

        EXPECT_TRUE(drm.receivedContextCreateFlags & I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS);

        auto extSetparam = drm.receivedContextCreateSetParam;

        EXPECT_EQ(static_cast<uint32_t>(I915_CONTEXT_CREATE_EXT_SETPARAM), extSetparam.base.name);
        EXPECT_EQ(0u, extSetparam.base.nextExtension);
        EXPECT_EQ(0u, extSetparam.base.flags);

        EXPECT_EQ(static_cast<uint64_t>(DrmPrelimHelper::getAccContextParam()), extSetparam.param.param);
        EXPECT_EQ(DrmPrelimHelper::getAccContextParamSize(), extSetparam.param.size);
        EXPECT_EQ(0u, extSetparam.param.contextId);
        EXPECT_NE(0u, extSetparam.param.value);

        auto paramAcc = drm.context.receivedContextParamAcc;
        ASSERT_TRUE(paramAcc);
        EXPECT_EQ(DrmPrelimHelper::getContextAcgValues()[1], paramAcc->granularity);
        EXPECT_EQ(1u, paramAcc->notify);
        EXPECT_EQ(threshold, paramAcc->trigger);
    }
}

TEST(DrmQueryTest, givenCreateContextWithAccessCounterWhenDrmContextIsCreatedThenProgramAccessCountersWithSpecifiedGranularity) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateContextWithAccessCounters.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    for (uint8_t granularity : DrmPrelimHelper::getContextAcgValues()) {
        debugManager.flags.AccessCountersGranularity.set(granularity);

        auto ret = drm.createDrmContext(0, false, false);
        EXPECT_EQ(0, ret);

        EXPECT_TRUE(drm.receivedContextCreateFlags & I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS);

        auto extSetparam = drm.receivedContextCreateSetParam;

        EXPECT_EQ(static_cast<uint32_t>(I915_CONTEXT_CREATE_EXT_SETPARAM), extSetparam.base.name);
        EXPECT_EQ(0u, extSetparam.base.nextExtension);
        EXPECT_EQ(0u, extSetparam.base.flags);

        EXPECT_EQ(static_cast<uint64_t>(DrmPrelimHelper::getAccContextParam()), extSetparam.param.param);
        EXPECT_EQ(DrmPrelimHelper::getAccContextParamSize(), extSetparam.param.size);
        EXPECT_EQ(0u, extSetparam.param.contextId);
        EXPECT_NE(0u, extSetparam.param.value);

        auto paramAcc = drm.context.receivedContextParamAcc;
        ASSERT_TRUE(paramAcc);
        EXPECT_EQ(1u, paramAcc->notify);
        EXPECT_EQ(0u, paramAcc->trigger);
        EXPECT_EQ(static_cast<uint8_t>(granularity), paramAcc->granularity);
    }
}

TEST(DrmQueryTest, givenPrelimEuDebugEnabledWhenCallingIsDebugAttachAvailableThenReturnValueIsTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.allowDebugAttachCallBase = true;
    drm.prelimEuDebugValue = 1;

    EXPECT_TRUE(drm.isDebugAttachAvailable());
}

TEST(DrmQueryTest, givenPrelimEuDebugDisabledWhenCallingIsDebugAttachAvailableThenReturnValueIsFalse) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.allowDebugAttachCallBase = true;
    drm.prelimEuDebugValue = 0;

    EXPECT_FALSE(drm.isDebugAttachAvailable());
}

TEST(DrmPrelimTest, GivenDebuggerOpenIoctlWhenErrorEbusyReturnedThenErrorIsReturnedWithoutReinvokingIoctl) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.allowDebugAttachCallBase = true;

    VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl, [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        return -1;
    });
    DrmDebuggerOpen open = {};
    open.pid = 1;
    open.events = 0;
    drm.context.debuggerOpenRetval = -1;
    drm.errnoRetVal = EBUSY;

    auto ret = drm.Drm::ioctl(DrmIoctl::debuggerOpen, &open);
    EXPECT_EQ(-1, ret);
}

TEST(DrmPrelimTest, GivenDebuggerOpenIoctlWhenErrorEAgainOrEIntrReturnedThenIoctlIsCalledAgain) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.allowDebugAttachCallBase = true;
    drm.baseErrno = false;

    DrmDebuggerOpen open = {};
    open.pid = 1;
    open.events = 0;

    {
        VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl, [](int fileDescriptor, unsigned long int request, void *arg) -> int {
            static int callCount = 3;
            if (callCount > 0) {
                callCount--;
                return -1;
            }
            return 0;
        });

        drm.errnoRetVal = EAGAIN;

        auto ret = drm.Drm::ioctl(DrmIoctl::debuggerOpen, &open);
        EXPECT_EQ(0, ret);
    }

    {
        VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl, [](int fileDescriptor, unsigned long int request, void *arg) -> int {
            static int callCount = 3;
            if (callCount > 0) {
                callCount--;
                return -1;
            }
            return 0;
        });

        drm.ioctlCallsCount = 0;
        drm.errnoRetVal = EINTR;

        auto ret = drm.Drm::ioctl(DrmIoctl::debuggerOpen, &open);
        EXPECT_EQ(0, ret);
    }
}

struct BufferObjectMock : public BufferObject {
    using BufferObject::BufferObject;
    using BufferObject::fillExecObject;
};

TEST(DrmBufferObjectTestPrelim, givenDisableScratchPagesWhenCreateDrmVirtualMemoryThenProperFlagIsSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableScratchPages.set(1);
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(0u);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();

    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);

    EXPECT_TRUE(drm.receivedGemVmControl.flags & DrmPrelimHelper::getDisableScratchVmCreateFlag());
}

TEST(DrmBufferObjectPrelim, givenDebuggingEnabledWithoutDisableScratchPagesFlagSetWhenCreateDrmVirtualMemoryThenDisableScratchPagesFlagIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(0u);
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    executionEnvironment->rootDeviceEnvironments[0]->productHelper.reset(productHelper.release());
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();
    EXPECT_TRUE(drm.disableScratch);
    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);
    EXPECT_TRUE(drm.receivedGemVmControl.flags & DrmPrelimHelper::getDisableScratchVmCreateFlag());
}

TEST(DrmBufferObjectPrelim, givenDebuggingEnabledWithDisableScratchPagesFlagSetWhenCreateDrmVirtualMemoryThenDisableScratchPagesFlagIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(0u);
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->isDisableScratchPagesRequiredForDebuggerResult = false;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    executionEnvironment->rootDeviceEnvironments[0]->productHelper.reset(productHelper.release());
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();
    EXPECT_FALSE(drm.disableScratch);
    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);
    EXPECT_FALSE(drm.receivedGemVmControl.flags & DrmPrelimHelper::getDisableScratchVmCreateFlag());
}

TEST(DrmBufferObjectTestPrelim, givenDisableScratchPagesDebugKeyOffAndDebuggingEnabledWhenCreateDrmVirtualMemoryThenEnvVariableIsPriority) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableScratchPages.set(0);
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(0u);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();

    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);

    EXPECT_FALSE(drm.receivedGemVmControl.flags & DrmPrelimHelper::getDisableScratchVmCreateFlag());
}

TEST(DrmBufferObjectTestPrelim, givenDisableScratchPagesAndDebuggingEnabledWhenCreateDrmVirtualMemoryThenEnvVariableIsPriority) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableScratchPages.set(1);
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(0u);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();

    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);

    EXPECT_TRUE(drm.receivedGemVmControl.flags & DrmPrelimHelper::getDisableScratchVmCreateFlag());
}

TEST(DrmBufferObjectTestPrelim, givenLocalMemoryDisabledWhenCreateDrmVirtualMemoryThenVmRegionExtensionIsNotPassed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(0);
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(1u);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);

    EXPECT_EQ(drm.receivedGemVmControl.extensions, 0ull);
}

TEST(DrmBufferObjectTestPrelim, givenLocalMemoryEnabledWhenCreateDrmVirtualMemoryThenVmRegionExtensionIsPassed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(1u);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    HardwareInfo testHwInfo = *defaultHwInfo;

    testHwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = true;
    testHwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 1;
    testHwInfo.gtSystemInfo.MultiTileArchInfo.Tile0 = 1;
    testHwInfo.gtSystemInfo.MultiTileArchInfo.TileMask = 0b1;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&testHwInfo);
    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);

    EXPECT_NE(drm.receivedGemVmControl.extensions, 0ull);
}

TEST(DrmBufferObjectTestPrelim, givenBufferObjectSetToColourWithBindWhenBindingThenSetProperAddressAndSize) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    BufferObjectMock bo(0u, &drm, 3, 1, 0, 1);
    bo.setColourWithBind();
    bo.setColourChunk(MemoryConstants::pageSize64k);
    bo.addColouringAddress(0xffeeffee);
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    bo.bind(&osContext, 0, false);
    ASSERT_TRUE(drm.context.receivedVmBind);
    EXPECT_EQ(drm.context.receivedVmBind->length, MemoryConstants::pageSize64k);
    EXPECT_EQ(drm.context.receivedVmBind->start, 0xffeeffee);
}

TEST(DrmBufferObjectTestPrelim, givenPageFaultNotSupportedWhenCallingCreateDrmVirtualMemoryThenDontEnablePageFaultsOnVirtualMemory) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    EXPECT_FALSE(drm.pageFaultSupported);

    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);

    EXPECT_FALSE(drm.receivedGemVmControl.flags & DrmPrelimHelper::getEnablePageFaultVmCreateFlag());

    drm.destroyDrmVirtualMemory(vmId);
}

TEST(DrmBufferObjectTestPrelim, givenPageFaultSupportedWhenVmBindIsAvailableThenEnablePageFaultsOnVirtualMemory) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.pageFaultSupported = true;

    for (auto vmBindAvailable : {false, true}) {
        drm.bindAvailable = vmBindAvailable;

        uint32_t vmId = 0;
        drm.ioctlCount.gemVmCreate = 0;
        drm.createDrmVirtualMemory(vmId);
        EXPECT_EQ(1, drm.ioctlCount.gemVmCreate.load());

        if (drm.isVmBindAvailable()) {
            EXPECT_TRUE(drm.receivedGemVmControl.flags & DrmPrelimHelper::getEnablePageFaultVmCreateFlag());
        } else {
            EXPECT_FALSE(drm.receivedGemVmControl.flags & DrmPrelimHelper::getEnablePageFaultVmCreateFlag());
        }

        drm.ioctlCount.gemVmDestroy = 0;
        drm.destroyDrmVirtualMemory(vmId);
        EXPECT_EQ(1, drm.ioctlCount.gemVmDestroy.load());
    }
}

TEST(DrmBufferObjectTestPrelim, givenContextWhenQueryingVmIdThenIoctlIsCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    BufferObjectMock bo(0u, &drm, 3, 1, 0, 1);
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    uint32_t vmId = 0;
    drm.storedRetValForVmId = 10;
    drm.queryVmId(0, vmId);

    EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_VM), drm.receivedContextParamRequest.param);
    EXPECT_EQ(static_cast<uint32_t>(drm.storedRetValForVmId), vmId);
}

TEST(DrmBufferObjectTestPrelim, givenBufferObjectMarkedForCaptureAndDebuggerEnabledWhenBindingThenCaptureFlagIsSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    BufferObjectMock bo(0, &drm, 3, 1, 0, 1);
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    bo.markForCapture();

    bo.bind(&osContext, 0, false);
    ASSERT_TRUE(drm.context.receivedVmBind);
    EXPECT_TRUE(drm.context.receivedVmBind->flags & DrmPrelimHelper::getCaptureVmBindFlag());
}

TEST(DrmBufferObjectTestPrelim, givenBufferObjectMarkedForCaptureAndDebuggerNotEnabledWhenBindingThenCaptureFlagIsNotSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    BufferObjectMock bo(0, &drm, 3, 1, 0, 1);
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    bo.markForCapture();

    bo.bind(&osContext, 0, false);
    ASSERT_TRUE(drm.context.receivedVmBind);
    EXPECT_FALSE(drm.context.receivedVmBind->flags & DrmPrelimHelper::getCaptureVmBindFlag());
}

TEST(DrmBufferObjectTestPrelim, givenNoActiveDirectSubmissionAndForceUseImmediateExtensionWhenBindingThenImmediateFlagIsSetAndExtensionListIsNotNull) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableImmediateVmBindExt.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    BufferObjectMock bo(0u, &drm, 3, 1, 0, 1);
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    bo.bind(&osContext, 0, false);
    ASSERT_TRUE(drm.context.receivedVmBind);
    EXPECT_TRUE(drm.context.receivedVmBind->flags & DrmPrelimHelper::getImmediateVmBindFlag());
    EXPECT_NE(drm.context.receivedVmBind->extensions, 0u);
}

TEST(DrmBufferObjectTestPrelim, whenBindingThenImmediateFlagIsSetAndExtensionListIsNotNull) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.setDirectSubmissionActive(true);
    BufferObjectMock bo(0u, &drm, 3, 1, 0, 1);
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    osContext.setDirectSubmissionActive();

    bo.bind(&osContext, 0, false);
    ASSERT_TRUE(drm.context.receivedVmBind);
    EXPECT_TRUE(drm.context.receivedVmBind->flags & DrmPrelimHelper::getImmediateVmBindFlag());
    EXPECT_NE(drm.context.receivedVmBind->extensions, 0u);
}

TEST(DrmBufferObjectTestPrelim, givenProvidedCtxIdWhenCallingWaitUserFenceThenExpectCtxFlagSetAndNoSoftFlagSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    uint64_t gpuAddress = 0x1020304000ull;
    uint64_t value = 0x98765ull;
    drm.waitUserFence(10u, gpuAddress, value, Drm::ValueWidth::u8, -1, 0u, false, NEO::InterruptId::notUsed, nullptr);

    EXPECT_EQ(1u, drm.context.waitUserFenceCalled);
    const auto &waitUserFence = drm.context.receivedWaitUserFence;
    ASSERT_TRUE(waitUserFence);
    EXPECT_EQ(10u, waitUserFence->ctxId);
    EXPECT_EQ(DrmPrelimHelper::getU8WaitUserFenceFlag(), waitUserFence->mask);
    EXPECT_EQ(gpuAddress, waitUserFence->addr);
    EXPECT_EQ(0u, waitUserFence->flags);
    EXPECT_EQ(value, waitUserFence->value);
    EXPECT_EQ(-1, waitUserFence->timeout);
}

TEST(DrmBufferObjectTestPrelim, givenProvidedNoCtxIdWhenCallingWaitUserFenceThenExpectCtxFlagNotSetAndSoftFlagSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    uint64_t gpuAddress = 0x1020304000ull;
    uint64_t value = 0x98765ull;
    drm.waitUserFence(0u, gpuAddress, value, Drm::ValueWidth::u16, 2, 3u, false, NEO::InterruptId::notUsed, nullptr);

    EXPECT_EQ(1u, drm.context.waitUserFenceCalled);
    const auto &waitUserFence = drm.context.receivedWaitUserFence;
    ASSERT_TRUE(waitUserFence);
    EXPECT_EQ(0u, waitUserFence->ctxId);
    EXPECT_EQ(DrmPrelimHelper::getU16WaitUserFenceFlag(), waitUserFence->mask);
    EXPECT_EQ(gpuAddress, waitUserFence->addr);
    EXPECT_EQ(3u, waitUserFence->flags);
    EXPECT_EQ(value, waitUserFence->value);
    EXPECT_EQ(2, waitUserFence->timeout);
}

TEST(DrmTestPrelim, givenHungContextWhenCallingWaitUserFenceThenSmallTimeoutIsPassed) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    OsContextLinux osContext(drm, 0, 10u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    osContext.setHangDetected();

    uint64_t memory = 0;
    uint64_t value = 20;
    drm.waitOnUserFences(osContext, reinterpret_cast<uint64_t>(&memory), value, 1, -1, 0, false, NEO::InterruptId::notUsed, nullptr);

    EXPECT_EQ(osContext.getDrmContextIds().size(), drm.context.waitUserFenceCalled);
    const auto &waitUserFence = drm.context.receivedWaitUserFence;
    ASSERT_TRUE(waitUserFence);
    EXPECT_EQ(DrmPrelimHelper::getU64WaitUserFenceFlag(), waitUserFence->mask);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&memory), waitUserFence->addr);
    EXPECT_EQ(0u, waitUserFence->flags);
    EXPECT_EQ(value, waitUserFence->value);
    EXPECT_EQ(1, waitUserFence->timeout);
}
