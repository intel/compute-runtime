/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/memory_constants.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/mocks/mock_wddm_interface23.h"
#include "unit_tests/os_interface/windows/gdi_dll_fixture.h"

using namespace NEO;

struct Wddm23TestsWithoutWddmInit : public ::testing::Test, GdiDllFixture {
    void SetUp() override {
        GdiDllFixture::SetUp();

        executionEnvironment = platformImpl->peekExecutionEnvironment();
        wddm = static_cast<WddmMock *>(Wddm::createWddm());
        osInterface = std::make_unique<OSInterface>();
        osInterface->get()->setWddm(wddm);

        wddm->featureTable->ftrWddmHwQueues = true;
        wddmMockInterface = new WddmMockInterface23(*wddm);
        wddm->wddmInterface.reset(wddmMockInterface);
        wddm->registryReader.reset(new RegistryReaderMock());
    }

    void init() {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
        auto hwInfo = *platformDevices[0];
        wddmMockInterface = reinterpret_cast<WddmMockInterface23 *>(wddm->wddmInterface.release());
        wddm->init(hwInfo);
        wddm->wddmInterface.reset(wddmMockInterface);
        osContext = std::make_unique<OsContextWin>(*wddm, 0u, 1, HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0], preemptionMode, false);
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
    auto defaultEngine = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0];
    OsContextWin osContextWithoutPreemption(*osInterface->get()->getWddm(), 0u, 1, defaultEngine, PreemptionMode::Disabled, false);
    OsContextWin osContextWithPreemption(*osInterface->get()->getWddm(), 0u, 1, defaultEngine, PreemptionMode::MidBatch, false);

    wddm->wddmInterface->createHwQueue(osContextWithoutPreemption);
    EXPECT_EQ(0u, getCreateHwQueueDataFcn()->Flags.DisableGpuTimeout);

    wddm->wddmInterface->createHwQueue(osContextWithPreemption);
    EXPECT_EQ(1u, getCreateHwQueueDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(Wddm23Tests, whenDestroyHwQueueCalledThenPassExistingHandle) {
    D3DKMT_HANDLE hwQueue = 123;
    osContext->setHwQueue(hwQueue);
    wddmMockInterface->destroyHwQueue(osContext->getHwQueue());
    EXPECT_EQ(hwQueue, getDestroyHwQueueDataFcn()->hHwQueue);

    hwQueue = 0;
    osContext->setHwQueue(hwQueue);
    wddmMockInterface->destroyHwQueue(osContext->getHwQueue());
    EXPECT_NE(hwQueue, getDestroyHwQueueDataFcn()->hHwQueue); // gdi not called when 0
}

TEST_F(Wddm23Tests, whenObjectIsDestructedThenDestroyHwQueue) {
    D3DKMT_HANDLE hwQueue = 123;
    osContext->setHwQueue(hwQueue);
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

    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader, *osContext);

    EXPECT_EQ(cmdBufferAddress, getSubmitCommandToHwQueueDataFcn()->CommandBuffer);
    EXPECT_EQ(static_cast<UINT>(cmdSize), getSubmitCommandToHwQueueDataFcn()->CommandLength);
    EXPECT_EQ(hwQueue, getSubmitCommandToHwQueueDataFcn()->hHwQueue);
    EXPECT_EQ(osContext->getResidencyController().getMonitoredFence().fenceHandle, getSubmitCommandToHwQueueDataFcn()->HwQueueProgressFenceId);
    EXPECT_EQ(&cmdBufferHeader, getSubmitCommandToHwQueueDataFcn()->pPrivateDriverData);
    EXPECT_EQ(static_cast<UINT>(MemoryConstants::pageSize), getSubmitCommandToHwQueueDataFcn()->PrivateDriverDataSize);

    EXPECT_EQ(osContext->getResidencyController().getMonitoredFence().gpuAddress, cmdBufferHeader.MonitorFenceVA);
    EXPECT_EQ(osContext->getResidencyController().getMonitoredFence().lastSubmittedFence, cmdBufferHeader.MonitorFenceValue);
    EXPECT_EQ(2u, osContext->getResidencyController().getMonitoredFence().currentFenceValue);
    EXPECT_EQ(1u, osContext->getResidencyController().getMonitoredFence().lastSubmittedFence);
}

TEST_F(Wddm23Tests, whenMonitoredFenceIsCreatedThenSetupAllRequiredFields) {
    wddm->wddmInterface->createMonitoredFence(osContext->getResidencyController());

    EXPECT_NE(nullptr, osContext->getResidencyController().getMonitoredFence().cpuAddress);
    EXPECT_EQ(1u, osContext->getResidencyController().getMonitoredFence().currentFenceValue);
    EXPECT_NE(static_cast<D3DKMT_HANDLE>(0), osContext->getResidencyController().getMonitoredFence().fenceHandle);
    EXPECT_NE(static_cast<D3DGPU_VIRTUAL_ADDRESS>(0), osContext->getResidencyController().getMonitoredFence().gpuAddress);
    EXPECT_EQ(0u, osContext->getResidencyController().getMonitoredFence().lastSubmittedFence);
}

TEST_F(Wddm23Tests, givenCurrentPendingFenceValueGreaterThanPendingFenceValueWhenSubmitCalledThenCallWaitOnGpu) {
    uint64_t cmdBufferAddress = 123;
    size_t cmdSize = 456;
    COMMAND_BUFFER_HEADER cmdBufferHeader = {};

    *wddm->pagingFenceAddress = 1;
    wddm->currentPagingFenceValue = 1;
    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader, *osContext);
    EXPECT_EQ(0u, wddm->waitOnGPUResult.called);

    wddm->currentPagingFenceValue = 2;
    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader, *osContext);
    EXPECT_EQ(1u, wddm->waitOnGPUResult.called);
}

TEST_F(Wddm23TestsWithoutWddmInit, whenInitCalledThenInitializeNewGdiDDIsAndCallToCreateHwQueue) {
    EXPECT_EQ(nullptr, wddm->gdi->createHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->gdi->destroyHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->gdi->submitCommandToHwQueue.mFunc);

    init();
    EXPECT_EQ(1u, wddmMockInterface->createHwQueueCalled);

    EXPECT_NE(nullptr, wddm->gdi->createHwQueue.mFunc);
    EXPECT_NE(nullptr, wddm->gdi->destroyHwQueue.mFunc);
    EXPECT_NE(nullptr, wddm->gdi->submitCommandToHwQueue.mFunc);
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
    wddm->gdi.reset(myMockGdi);
    init();
    EXPECT_FALSE(osContext->isInitialized());
    EXPECT_EQ(1u, wddmMockInterface->createHwQueueCalled);
    EXPECT_FALSE(wddmMockInterface->createHwQueueResult);
}
