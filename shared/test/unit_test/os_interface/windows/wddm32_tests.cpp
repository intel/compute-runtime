/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/windows/wddm32_tests.h"

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"

using namespace NEO;

TEST_F(Wddm32Tests, whenGetDedicatedVideoMemoryIsCalledThenCorrectValueIsReturned) {
    EXPECT_EQ(wddm->dedicatedVideoMemory, wddm->getDedicatedVideoMemory());
}

TEST_F(Wddm32Tests, whenCreateContextIsCalledThenEnableHwQueues) {
    EXPECT_TRUE(wddm->wddmInterface->hwQueuesSupported());
    EXPECT_EQ(1u, getCreateContextDataFcn()->Flags.HwQueueSupported);
}

TEST_F(Wddm32Tests, givenPreemptionModeWhenCreateHwQueueCalledThenSetGpuTimeoutIfEnabled) {
    auto &gfxCoreHelper = this->executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto defaultEngine = gfxCoreHelper.getGpgpuEngineInstances(*this->executionEnvironment.rootDeviceEnvironments[0])[0];
    OsContextWin osContextWithoutPreemption(*wddm, 0, 0u,
                                            EngineDescriptorHelper::getDefaultDescriptor(defaultEngine, PreemptionMode::Disabled));
    OsContextWin osContextWithPreemption(*wddm, 0, 0, EngineDescriptorHelper::getDefaultDescriptor(defaultEngine, PreemptionMode::MidBatch));

    wddm->wddmInterface->createHwQueue(osContextWithoutPreemption);
    EXPECT_EQ(0u, getCreateHwQueueDataFcn()->Flags.DisableGpuTimeout);

    wddm->wddmInterface->createHwQueue(osContextWithPreemption);
    EXPECT_EQ(1u, getCreateHwQueueDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(Wddm32Tests, whenDestroyHwQueueCalledThenPassExistingHandle) {
    D3DKMT_HANDLE hwQueue = 123;
    osContext->setHwQueue({hwQueue, 0, nullptr, 0});
    wddmMockInterface->destroyHwQueue(osContext->getHwQueue().handle);
    EXPECT_EQ(hwQueue, getDestroyHwQueueDataFcn()->hHwQueue);

    hwQueue = 0;
    osContext->setHwQueue({hwQueue, 0, nullptr, 0});
    wddmMockInterface->destroyHwQueue(osContext->getHwQueue().handle);
    EXPECT_NE(hwQueue, getDestroyHwQueueDataFcn()->hHwQueue); // gdi not called when 0
}

TEST_F(Wddm32Tests, whenObjectIsDestructedThenDestroyHwQueue) {
    D3DKMT_HANDLE hwQueue = 123;
    osContext->setHwQueue({hwQueue, 0, nullptr, 0});
    osContext.reset();
    EXPECT_EQ(hwQueue, getDestroyHwQueueDataFcn()->hHwQueue);
}

TEST_F(Wddm32Tests, givenCmdBufferWhenSubmitCalledThenSetAllRequiredFiledsAndUpdateMonitoredFence) {
    uint64_t cmdBufferAddress = 123;
    size_t cmdSize = 456;
    auto hwQueue = osContext->getHwQueue();
    COMMAND_BUFFER_HEADER cmdBufferHeader = {};

    EXPECT_EQ(1u, osContext->getResidencyController().getMonitoredFence().currentFenceValue);
    EXPECT_EQ(0u, osContext->getResidencyController().getMonitoredFence().lastSubmittedFence);

    WddmSubmitArguments submitArgs = {};
    submitArgs.contextHandle = osContext->getWddmContextHandle();
    submitArgs.hwQueueHandle = hwQueue.handle;
    submitArgs.monitorFence = &osContext->getResidencyController().getMonitoredFence();
    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader, submitArgs);

    EXPECT_EQ(cmdBufferAddress, getSubmitCommandToHwQueueDataFcn()->CommandBuffer);
    EXPECT_EQ(static_cast<UINT>(cmdSize), getSubmitCommandToHwQueueDataFcn()->CommandLength);
    EXPECT_EQ(hwQueue.handle, getSubmitCommandToHwQueueDataFcn()->hHwQueue);
    EXPECT_EQ(osContext->getResidencyController().getMonitoredFence().lastSubmittedFence, getSubmitCommandToHwQueueDataFcn()->HwQueueProgressFenceId);
    EXPECT_EQ(&cmdBufferHeader, getSubmitCommandToHwQueueDataFcn()->pPrivateDriverData);
    EXPECT_EQ(static_cast<UINT>(sizeof(COMMAND_BUFFER_HEADER)), getSubmitCommandToHwQueueDataFcn()->PrivateDriverDataSize);

    EXPECT_EQ(0u, cmdBufferHeader.MonitorFenceVA);
    EXPECT_EQ(0u, cmdBufferHeader.MonitorFenceValue);
    EXPECT_EQ(2u, osContext->getResidencyController().getMonitoredFence().currentFenceValue);
    EXPECT_EQ(1u, osContext->getResidencyController().getMonitoredFence().lastSubmittedFence);
}

TEST_F(Wddm32Tests, givenDebugVariableSetWhenSubmitCalledThenUseCmdBufferHeaderSizeForPrivateDriverDataSize) {
    DebugManagerStateRestore restore;
    debugManager.flags.UseCommandBufferHeaderSizeForWddmQueueSubmission.set(true);

    COMMAND_BUFFER_HEADER cmdBufferHeader = {};

    WddmSubmitArguments submitArgs = {};
    submitArgs.contextHandle = osContext->getWddmContextHandle();
    submitArgs.hwQueueHandle = osContext->getHwQueue().handle;
    submitArgs.monitorFence = &osContext->getResidencyController().getMonitoredFence();
    wddm->submit(123, 456, &cmdBufferHeader, submitArgs);

    EXPECT_EQ(static_cast<UINT>(sizeof(COMMAND_BUFFER_HEADER)), getSubmitCommandToHwQueueDataFcn()->PrivateDriverDataSize);

    debugManager.flags.UseCommandBufferHeaderSizeForWddmQueueSubmission.set(false);

    cmdBufferHeader = {};
    submitArgs = {};
    submitArgs.contextHandle = osContext->getWddmContextHandle();
    submitArgs.hwQueueHandle = osContext->getHwQueue().handle;
    submitArgs.monitorFence = &osContext->getResidencyController().getMonitoredFence();
    wddm->submit(123, 456, &cmdBufferHeader, submitArgs);

    EXPECT_EQ(static_cast<UINT>(MemoryConstants::pageSize), getSubmitCommandToHwQueueDataFcn()->PrivateDriverDataSize);
}

TEST_F(Wddm32Tests, whenMonitoredFenceIsCreatedThenSetupAllRequiredFields) {
    wddm->wddmInterface->createMonitoredFence(*osContext);
    auto hwQueue = osContext->getHwQueue();

    EXPECT_EQ(hwQueue.progressFenceCpuVA, osContext->getResidencyController().getMonitoredFence().cpuAddress);
    EXPECT_EQ(1u, osContext->getResidencyController().getMonitoredFence().currentFenceValue);
    EXPECT_EQ(hwQueue.progressFenceHandle, osContext->getResidencyController().getMonitoredFence().fenceHandle);
    EXPECT_EQ(hwQueue.progressFenceGpuVA, osContext->getResidencyController().getMonitoredFence().gpuAddress);
    EXPECT_EQ(0u, osContext->getResidencyController().getMonitoredFence().lastSubmittedFence);
}

TEST_F(Wddm32Tests, givenCurrentPendingFenceValueGreaterThanPendingFenceValueWhenSubmitCalledThenCallWaitOnGpu) {
    uint64_t cmdBufferAddress = 123;
    size_t cmdSize = 456;
    COMMAND_BUFFER_HEADER cmdBufferHeader = {};

    WddmSubmitArguments submitArgs = {};
    submitArgs.contextHandle = osContext->getWddmContextHandle();
    submitArgs.hwQueueHandle = osContext->getHwQueue().handle;
    submitArgs.monitorFence = &osContext->getResidencyController().getMonitoredFence();

    VariableBackup<volatile uint64_t> pagingFenceBackup(wddm->pagingFenceAddress);
    *wddm->pagingFenceAddress = 1;
    wddm->currentPagingFenceValue = 1;

    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader, submitArgs);
    EXPECT_EQ(0u, wddm->waitOnGPUResult.called);

    wddm->currentPagingFenceValue = 2;
    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader, submitArgs);
    EXPECT_EQ(1u, wddm->waitOnGPUResult.called);
}

TEST_F(Wddm32Tests, givenDestructionOsContextWinWhenCallingDestroyMonitorFenceThenDoNotCallDestroy) {
    osContext.reset(nullptr);
    EXPECT_EQ(0u, wddmMockInterface->destroyMonitorFenceCalled);
    EXPECT_EQ(0u, getDestroySynchronizationObjectDataFcn()->hSyncObject);
}

TEST_F(Wddm32TestsWithoutWddmInit, whenInitCalledThenInitializeNewGdiDDIsAndCallToCreateHwQueue) {
    EXPECT_EQ(nullptr, wddm->getGdi()->createHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->getGdi()->destroyHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->getGdi()->submitCommandToHwQueue.mFunc);

    init();
    EXPECT_EQ(1u, wddmMockInterface->createHwQueueCalled);

    EXPECT_NE(nullptr, wddm->getGdi()->createHwQueue.mFunc);
    EXPECT_NE(nullptr, wddm->getGdi()->destroyHwQueue.mFunc);
    EXPECT_NE(nullptr, wddm->getGdi()->submitCommandToHwQueue.mFunc);
}

TEST_F(Wddm32TestsWithoutWddmInit, whenCreateHwQueueFailedThenReturnFalseFromInit) {
    wddmMockInterface->forceCreateHwQueueFail = true;
    EXPECT_ANY_THROW(init());
}

TEST_F(Wddm32TestsWithoutWddmInit, givenFailureOnGdiInitializationWhenCreatingHwQueueThenReturnFailure) {
    struct MyMockGdi : public Gdi {
        bool setupHwQueueProcAddresses() override {
            return false;
        }
    };
    auto myMockGdi = new MyMockGdi();
    wddm->resetGdi(myMockGdi);
    EXPECT_ANY_THROW(init());
    EXPECT_EQ(1u, wddmMockInterface->createHwQueueCalled);
    EXPECT_FALSE(wddmMockInterface->createHwQueueResult);
}
