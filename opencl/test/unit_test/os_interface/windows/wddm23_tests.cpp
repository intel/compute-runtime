/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/mock_wddm.h"
#include "opencl/test/unit_test/mocks/mock_wddm_interface23.h"
#include "opencl/test/unit_test/os_interface/windows/gdi_dll_fixture.h"
#include "test.h"

using namespace NEO;

struct Wddm23TestsWithoutWddmInit : public ::testing::Test, GdiDllFixture {
    void SetUp() override {
        GdiDllFixture::SetUp();

        executionEnvironment = platform()->peekExecutionEnvironment();
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[0].get()));
        osInterface = std::make_unique<OSInterface>();
        osInterface->get()->setWddm(wddm);

        wddm->featureTable->ftrWddmHwQueues = true;
        wddmMockInterface = new WddmMockInterface23(*wddm);
        wddm->wddmInterface.reset(wddmMockInterface);
        wddm->registryReader.reset(new RegistryReaderMock());
    }

    void init() {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddmMockInterface = static_cast<WddmMockInterface23 *>(wddm->wddmInterface.release());
        wddm->init();
        wddm->wddmInterface.reset(wddmMockInterface);
        osContext = std::make_unique<OsContextWin>(*wddm, 0u, 1,
                                                   HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
                                                   preemptionMode, false, false, false);
    }

    void TearDown() override {
        GdiDllFixture::TearDown();
    }

    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<OsContextWin> osContext;
    WddmMock *wddm = nullptr;
    WddmMockInterface23 *wddmMockInterface = nullptr;
    ExecutionEnvironment *executionEnvironment;
};

struct Wddm23Tests : public Wddm23TestsWithoutWddmInit {
    using Wddm23TestsWithoutWddmInit::TearDown;
    void SetUp() override {
        Wddm23TestsWithoutWddmInit::SetUp();
        init();
    }
};

TEST_F(Wddm23Tests, whenGetDedicatedVideoMemoryIsCalledThenCorrectValueIsReturned) {
    EXPECT_EQ(wddm->dedicatedVideoMemory, wddm->getDedicatedVideoMemory());
}

TEST_F(Wddm23Tests, whenCreateContextIsCalledThenEnableHwQueues) {
    EXPECT_TRUE(wddm->wddmInterface->hwQueuesSupported());
    EXPECT_EQ(1u, getCreateContextDataFcn()->Flags.HwQueueSupported);
}

TEST_F(Wddm23Tests, givenPreemptionModeWhenCreateHwQueueCalledThenSetGpuTimeoutIfEnabled) {
    auto defaultEngine = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0];
    OsContextWin osContextWithoutPreemption(*osInterface->get()->getWddm(), 0u, 1, defaultEngine, PreemptionMode::Disabled,
                                            false, false, false);
    OsContextWin osContextWithPreemption(*osInterface->get()->getWddm(), 0u, 1, defaultEngine, PreemptionMode::MidBatch,
                                         false, false, false);

    wddm->wddmInterface->createHwQueue(osContextWithoutPreemption);
    EXPECT_EQ(0u, getCreateHwQueueDataFcn()->Flags.DisableGpuTimeout);

    wddm->wddmInterface->createHwQueue(osContextWithPreemption);
    EXPECT_EQ(1u, getCreateHwQueueDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(Wddm23Tests, whenDestroyHwQueueCalledThenPassExistingHandle) {
    D3DKMT_HANDLE hwQueue = 123;
    osContext->setHwQueue({hwQueue, 0, nullptr, 0});
    wddmMockInterface->destroyHwQueue(osContext->getHwQueue().handle);
    EXPECT_EQ(hwQueue, getDestroyHwQueueDataFcn()->hHwQueue);

    hwQueue = 0;
    osContext->setHwQueue({hwQueue, 0, nullptr, 0});
    wddmMockInterface->destroyHwQueue(osContext->getHwQueue().handle);
    EXPECT_NE(hwQueue, getDestroyHwQueueDataFcn()->hHwQueue); // gdi not called when 0
}

TEST_F(Wddm23Tests, whenObjectIsDestructedThenDestroyHwQueue) {
    D3DKMT_HANDLE hwQueue = 123;
    osContext->setHwQueue({hwQueue, 0, nullptr, 0});
    osContext.reset();
    EXPECT_EQ(hwQueue, getDestroyHwQueueDataFcn()->hHwQueue);
}

TEST_F(Wddm23Tests, givenCmdBufferWhenSubmitCalledThenSetAllRequiredFiledsAndUpdateMonitoredFence) {
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
    EXPECT_EQ(static_cast<UINT>(MemoryConstants::pageSize), getSubmitCommandToHwQueueDataFcn()->PrivateDriverDataSize);

    EXPECT_EQ(0u, cmdBufferHeader.MonitorFenceVA);
    EXPECT_EQ(0u, cmdBufferHeader.MonitorFenceValue);
    EXPECT_EQ(2u, osContext->getResidencyController().getMonitoredFence().currentFenceValue);
    EXPECT_EQ(1u, osContext->getResidencyController().getMonitoredFence().lastSubmittedFence);
}

TEST_F(Wddm23Tests, whenMonitoredFenceIsCreatedThenSetupAllRequiredFields) {
    wddm->wddmInterface->createMonitoredFence(*osContext);
    auto hwQueue = osContext->getHwQueue();

    EXPECT_EQ(hwQueue.progressFenceCpuVA, osContext->getResidencyController().getMonitoredFence().cpuAddress);
    EXPECT_EQ(1u, osContext->getResidencyController().getMonitoredFence().currentFenceValue);
    EXPECT_EQ(hwQueue.progressFenceHandle, osContext->getResidencyController().getMonitoredFence().fenceHandle);
    EXPECT_EQ(hwQueue.progressFenceGpuVA, osContext->getResidencyController().getMonitoredFence().gpuAddress);
    EXPECT_EQ(0u, osContext->getResidencyController().getMonitoredFence().lastSubmittedFence);
}

TEST_F(Wddm23Tests, givenCurrentPendingFenceValueGreaterThanPendingFenceValueWhenSubmitCalledThenCallWaitOnGpu) {
    uint64_t cmdBufferAddress = 123;
    size_t cmdSize = 456;
    COMMAND_BUFFER_HEADER cmdBufferHeader = {};

    WddmSubmitArguments submitArgs = {};
    submitArgs.contextHandle = osContext->getWddmContextHandle();
    submitArgs.hwQueueHandle = osContext->getHwQueue().handle;
    submitArgs.monitorFence = &osContext->getResidencyController().getMonitoredFence();

    *wddm->pagingFenceAddress = 1;
    wddm->currentPagingFenceValue = 1;

    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader, submitArgs);
    EXPECT_EQ(0u, wddm->waitOnGPUResult.called);

    wddm->currentPagingFenceValue = 2;
    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader, submitArgs);
    EXPECT_EQ(1u, wddm->waitOnGPUResult.called);
}

TEST_F(Wddm23Tests, givenDestructionOsContextWinWhenCallingDestroyMonitorFenceThenDoNotCallGdiDestroy) {
    osContext.reset(nullptr);
    EXPECT_EQ(1u, wddmMockInterface->destroyMonitorFenceCalled);
    EXPECT_EQ(0u, getDestroySynchronizationObjectDataFcn()->hSyncObject);
}

TEST_F(Wddm23TestsWithoutWddmInit, whenInitCalledThenInitializeNewGdiDDIsAndCallToCreateHwQueue) {
    EXPECT_EQ(nullptr, wddm->getGdi()->createHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->getGdi()->destroyHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->getGdi()->submitCommandToHwQueue.mFunc);

    init();
    EXPECT_EQ(1u, wddmMockInterface->createHwQueueCalled);

    EXPECT_NE(nullptr, wddm->getGdi()->createHwQueue.mFunc);
    EXPECT_NE(nullptr, wddm->getGdi()->destroyHwQueue.mFunc);
    EXPECT_NE(nullptr, wddm->getGdi()->submitCommandToHwQueue.mFunc);
}

TEST_F(Wddm23TestsWithoutWddmInit, whenCreateHwQueueFailedThenReturnFalseFromInit) {
    wddmMockInterface->forceCreateHwQueueFail = true;
    init();
    EXPECT_FALSE(osContext->isInitialized());
}

TEST_F(Wddm23TestsWithoutWddmInit, givenFailureOnGdiInitializationWhenCreatingHwQueueThenReturnFailure) {
    struct MyMockGdi : public Gdi {
        bool setupHwQueueProcAddresses() override {
            return false;
        }
    };
    auto myMockGdi = new MyMockGdi();
    wddm->resetGdi(myMockGdi);
    init();
    EXPECT_FALSE(osContext->isInitialized());
    EXPECT_EQ(1u, wddmMockInterface->createHwQueueCalled);
    EXPECT_FALSE(wddmMockInterface->createHwQueueResult);
}
