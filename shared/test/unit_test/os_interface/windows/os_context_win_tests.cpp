/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/mock_wddm_interface.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"

using namespace NEO;

struct OsContextWinTest : public WddmTestWithMockGdiDll {
    void SetUp() override {
        WddmTestWithMockGdiDll::SetUp();
        preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        auto &gfxCoreHelper = this->rootDeviceEnvironment->getHelper<GfxCoreHelper>();
        engineTypeUsage = gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment)[0];

        init();
    }

    PreemptionMode preemptionMode;
    EngineTypeUsage engineTypeUsage;
};

TEST_F(OsContextWinTest, givenWddm20WhenCreatingOsContextThenOsContextIsInitialized) {
    osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage, preemptionMode));
    EXPECT_NO_THROW(osContext->ensureContextInitialized(false));
}

TEST_F(OsContextWinTest, givenWddm20WhenCreatingWddmContextFailThenOsContextCreationFails) {
    wddm->device = INVALID_HANDLE;
    osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage, preemptionMode));
    EXPECT_ANY_THROW(osContext->ensureContextInitialized(false));
}

TEST_F(OsContextWinTest, givenWddm20WhenCreatingWddmMonitorFenceFailThenOsContextCreationFails) {
    *getCreateSynchronizationObject2FailCallFcn() = true;
    osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage, preemptionMode));
    EXPECT_ANY_THROW(osContext->ensureContextInitialized(false));
}

TEST_F(OsContextWinTest, givenWddm20WhenRegisterTrimCallbackFailThenResidencyControllerIsNotInitialized) {
    *getRegisterTrimNotificationFailCallFcn() = true;
    WddmMock wddm(*rootDeviceEnvironment);
    wddm.init();

    wddm.getResidencyController().registerCallback();
    EXPECT_FALSE(wddm.getResidencyController().isInitialized());
}

TEST_F(OsContextWinTest, givenWddm20WhenRegisterTrimCallbackIsDisabledThenOsContextIsInitialized) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.DoNotRegisterTrimCallback.set(true);
    *getRegisterTrimNotificationFailCallFcn() = true;

    osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage, preemptionMode));
    EXPECT_NO_THROW(osContext->ensureContextInitialized(false));
}

TEST_F(OsContextWinTest, givenReinitializeContextWhenContextIsInitThenContextIsDestroyedAndRecreated) {
    osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage, preemptionMode));
    EXPECT_NO_THROW(osContext->reInitializeContext());
    EXPECT_NO_THROW(osContext->ensureContextInitialized(false));
}

TEST_F(OsContextWinTest, givenReinitializeContextWhenContextIsNotInitThenContextIsCreated) {
    EXPECT_NO_THROW(osContext->reInitializeContext());
    EXPECT_NO_THROW(osContext->ensureContextInitialized(false));
}

TEST_F(OsContextWinTest, givenOsContextWinWhenQueryingForOfflineDumpContextIdThenCorrectValueIsReturned) {
    osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage, preemptionMode));
    EXPECT_EQ(0u, osContext->getOfflineDumpContextId(0));
}

TEST_F(OsContextWinTest, givenWddm20AndProductSupportDirectSubmissionThenDirectSubmissionIsSupported) {
    auto &productHelper = this->rootDeviceEnvironment->getHelper<ProductHelper>();
    osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage, preemptionMode));
    EXPECT_EQ(productHelper.isDirectSubmissionSupported(), osContext->isDirectSubmissionSupported());
}

TEST_F(OsContextWinTest, givenWddm23AndProductSupportDirectSubmissionThenDirectSubmissionIsSupported) {
    struct WddmMock : public Wddm {
        using Wddm::featureTable;
    };
    auto wddm = static_cast<WddmMock *>(osInterface->getDriverModel()->as<Wddm>());
    wddm->featureTable.get()->flags.ftrWddmHwQueues = 1;
    osContext = std::make_unique<OsContextWin>(*wddm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage, preemptionMode));
    EXPECT_EQ(this->rootDeviceEnvironment->getHelper<ProductHelper>().isDirectSubmissionSupported(), osContext->isDirectSubmissionSupported());
}

TEST_F(OsContextWinTest, givenWddmOnLinuxThenDirectSubmissionIsNotSupported) {
    struct MockRootDeviceEnvironment : public RootDeviceEnvironment {
        using RootDeviceEnvironment::isWddmOnLinuxEnable;
    };
    static_cast<MockRootDeviceEnvironment *>(this->rootDeviceEnvironment)->isWddmOnLinuxEnable = true;
    osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage, preemptionMode));
    EXPECT_FALSE(osContext->isDirectSubmissionSupported());
}

TEST_F(OsContextWinTest, givenWddmWhenReinitializeCalledThenHwQueueDestroyCalled) {
    auto wddm = static_cast<WddmMock *>(osInterface->getDriverModel()->as<Wddm>());
    auto mockWddmInterface = std::make_unique<WddmMockInterface>(*wddm);
    auto pMockWddmInterface = mockWddmInterface.get();
    wddm->wddmInterface.reset(mockWddmInterface.release());
    osContext->reInitializeContext();
    EXPECT_EQ(pMockWddmInterface->destroyHwQueueCalled, 1u);
}

TEST_F(OsContextWinTest, givenWddmWithHwQueuesEnabledWhenReinitializeCalledThenCreateHwQueueCalled) {
    auto wddm = static_cast<WddmMock *>(osInterface->getDriverModel()->as<Wddm>());
    auto mockWddmInterface = std::make_unique<WddmMockInterface>(*wddm);
    mockWddmInterface->hwQueuesSupportedResult = true;
    auto pMockWddmInterface = mockWddmInterface.get();
    wddm->wddmInterface.reset(mockWddmInterface.release());
    osContext->reInitializeContext();
    EXPECT_EQ(pMockWddmInterface->createHwQueueCalled, 1u);
}
TEST_F(OsContextWinTest, givenWddmWithHwQueuesEnabledWhenReinitializeCalledThenCreateMonitorFenceCalled) {
    auto wddm = static_cast<WddmMock *>(osInterface->getDriverModel()->as<Wddm>());
    auto mockWddmInterface = std::make_unique<WddmMockInterface>(*wddm);
    mockWddmInterface->hwQueuesSupportedResult = true;
    auto pMockWddmInterface = mockWddmInterface.get();
    wddm->wddmInterface.reset(mockWddmInterface.release());
    osContext->reInitializeContext();
    EXPECT_EQ(pMockWddmInterface->createMonitoredFenceCalled, 1u);
}

TEST_F(OsContextWinTest, givenWddmWithHwQueuesDisabledWhenReinitializeCalledThenCreateHwQueueNotCalled) {
    auto wddm = static_cast<WddmMock *>(osInterface->getDriverModel()->as<Wddm>());
    auto mockWddmInterface = std::make_unique<WddmMockInterface>(*wddm);
    mockWddmInterface->hwQueuesSupportedResult = false;
    auto pMockWddmInterface = mockWddmInterface.get();
    wddm->wddmInterface.reset(mockWddmInterface.release());
    osContext->reInitializeContext();
    EXPECT_EQ(pMockWddmInterface->createHwQueueCalled, 0u);
}
TEST_F(OsContextWinTest, givenWddmWithHwQueuesDisabledWhenReinitializeCalledThenCreateMonitorFenceNotCalled) {
    auto wddm = static_cast<WddmMock *>(osInterface->getDriverModel()->as<Wddm>());
    auto mockWddmInterface = std::make_unique<WddmMockInterface>(*wddm);
    mockWddmInterface->hwQueuesSupportedResult = false;
    auto pMockWddmInterface = mockWddmInterface.get();
    wddm->wddmInterface.reset(mockWddmInterface.release());
    osContext->reInitializeContext();
    EXPECT_EQ(pMockWddmInterface->createMonitoredFenceCalled, 0u);
}

TEST_F(OsContextWinTest, whenCreatingContextThenLatePreemptionStartIsProperlyInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideLatePreemptionStart.set(1);
    auto makeOsContext = [&](EngineTypeUsage engineTypeUsage) {
        return std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage, PreemptionMode::MidThread));
    };

    // late start preemption initialized correctly
    auto osContext = makeOsContext({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular});
    EXPECT_TRUE(osContext->checkLatePreemptionStartSupport());

    // unsupported engine usage
    osContext = makeOsContext({aub_stream::EngineType::ENGINE_CCS, EngineUsage::internal});
    EXPECT_FALSE(osContext->checkLatePreemptionStartSupport());

    // unsupported engine type
    osContext = makeOsContext({aub_stream::EngineType::ENGINE_CCS1, EngineUsage::regular});
    EXPECT_FALSE(osContext->checkLatePreemptionStartSupport());

    // feature disabled
    debugManager.flags.OverrideLatePreemptionStart.set(0);
    osContext = makeOsContext({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular});
    EXPECT_FALSE(osContext->checkLatePreemptionStartSupport());
}

TEST_F(OsContextWinTest, whenInitPrivateDataIsCalledTwiceWithoutReInitThenErrorIsPropagated) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideLatePreemptionStart.set(1);

    VariableBackup<decltype(NEO::pCallEscape)> mockCallEscape(&NEO::pCallEscape, [](D3DKMT_ESCAPE &escapeCommand) -> NTSTATUS {
        return STATUS_SUCCESS;
    });
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);

    {
        debugManager.flags.OverrideLatePreemptionStart.set(0);
        auto osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}, PreemptionMode::MidThread));
        osContext->ensureContextInitialized(false);
        osContext->setCommandStreamReceiver(csr);
        EXPECT_EQ(static_cast<HANDLE>(NULL), osContext->latePreemptionStartEventHandle);

        debugManager.flags.OverrideLatePreemptionStart.set(1);
        osContext->reInitializeContext();
        EXPECT_NE(static_cast<HANDLE>(NULL), osContext->latePreemptionStartEventHandle);

        EXPECT_NO_THROW(osContext->reInitializeContext());
    }

    {
        debugManager.flags.OverrideLatePreemptionStart.set(1);
        auto osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}, PreemptionMode::MidThread));
        osContext->setCommandStreamReceiver(csr);
        EXPECT_EQ(static_cast<HANDLE>(NULL), osContext->latePreemptionStartEventHandle);

        initPrivateData(*osContext);
        EXPECT_NE(static_cast<HANDLE>(NULL), osContext->latePreemptionStartEventHandle);

        EXPECT_ANY_THROW(initPrivateData(*osContext));
    }
}

TEST_F(OsContextWinTest, whenLatePreemptionStartEventIsSignalledThenCommandsAreProgrammed) {
    DebugManagerStateRestore restorer;
    VariableBackup<decltype(NEO::pCallEscape)> mockCallEscape(&NEO::pCallEscape, [](D3DKMT_ESCAPE &escapeCommand) -> NTSTATUS {
        return STATUS_SUCCESS;
    });
    VariableBackup<decltype(SysCalls::sysCallsRegisterWaitForSingleObject)> mockSysCallsRegisterWaitForSingleObject(
        &SysCalls::sysCallsRegisterWaitForSingleObject,
        [](PHANDLE phNewWaitObject, HANDLE hObject, WAITORTIMERCALLBACK callback, PVOID context, ULONG dwMilliseconds, ULONG dwFlags) -> BOOL {
            callback(context, FALSE);
            return TRUE;
        });
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);

    auto osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}, PreemptionMode::MidThread));
    osContext->setCommandStreamReceiver(csr);

    auto privateData = initPrivateData(*osContext);
    EXPECT_FALSE(privateData.DisableWmtp);
    EXPECT_FALSE(privateData.NotifyPreemptExceedThreshold);
    EXPECT_EQ(0u, csr.submitLateMidThreadPreemptionStartCounter);

    debugManager.flags.OverrideLatePreemptionStart.set(1);
    privateData = initPrivateData(*osContext);
    EXPECT_TRUE(privateData.DisableWmtp);
    EXPECT_TRUE(privateData.NotifyPreemptExceedThreshold);
    EXPECT_EQ(1u, csr.submitLateMidThreadPreemptionStartCounter);
}

TEST_F(OsContextWinTest, whenStopLatePreemptionStartWaitThenResourcesAreFreed) {
    static int unregisterWaitCalled;
    unregisterWaitCalled = 0;
    SysCalls::closeHandleCalled = 0;
    destroySynchronizationObjectCallCount = 0;

    VariableBackup<decltype(SysCalls::sysCallsUnregisterWait)> mockSysCallsUnregisterWait(&SysCalls::sysCallsUnregisterWait, [](HANDLE waitHandle) -> BOOL {
        unregisterWaitCalled++;
        return TRUE;
    });

    auto osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}, PreemptionMode::MidThread));
    osContext->stopLatePreemptionStartWait();
    EXPECT_EQ(0, unregisterWaitCalled);
    EXPECT_EQ(0u, SysCalls::closeHandleCalled);
    EXPECT_EQ(0u, destroySynchronizationObjectCallCount);

    osContext->latePreemptionStartWaitObjectHandle = reinterpret_cast<HANDLE>(0xFFFF);
    osContext->stopLatePreemptionStartWait();
    EXPECT_EQ(1, unregisterWaitCalled);
    EXPECT_EQ(0u, SysCalls::closeHandleCalled);
    EXPECT_EQ(0u, destroySynchronizationObjectCallCount);
    EXPECT_EQ(static_cast<HANDLE>(NULL), osContext->latePreemptionStartWaitObjectHandle);

    osContext->latePreemptionStartEventHandle = reinterpret_cast<HANDLE>(dummyHandle);
    osContext->stopLatePreemptionStartWait();
    EXPECT_EQ(1, unregisterWaitCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
    EXPECT_EQ(0u, destroySynchronizationObjectCallCount);
    EXPECT_EQ(static_cast<HANDLE>(NULL), osContext->latePreemptionStartEventHandle);

    osContext->latePreemptionStartSyncObjectHandle = static_cast<D3DKMT_HANDLE>(0x1234);
    osContext->stopLatePreemptionStartWait();
    EXPECT_EQ(1, unregisterWaitCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
    EXPECT_EQ(1u, destroySynchronizationObjectCallCount);
    EXPECT_EQ(static_cast<D3DKMT_HANDLE>(0x1234), destroySynchronizationObjectData.hSyncObject);
    EXPECT_EQ(static_cast<D3DKMT_HANDLE>(NULL), osContext->latePreemptionStartSyncObjectHandle);
}

struct OsContextWinTestNoCleanup : public WddmTestWithMockGdiDllNoCleanup {
    void SetUp() override {
        WddmTestWithMockGdiDllNoCleanup::SetUp();
        preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);

        auto &gfxCoreHelper = this->rootDeviceEnvironment->getHelper<GfxCoreHelper>();
        engineTypeUsage = gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment)[0];

        init();
    }

    PreemptionMode preemptionMode;
    EngineTypeUsage engineTypeUsage;
};

TEST_F(OsContextWinTestNoCleanup, givenReinitializeContextWhenContextIsInitThenContextIsNotDestroyed) {
    osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage, preemptionMode));
    EXPECT_FALSE(this->wddm->isDriverAvailable());
    EXPECT_TRUE(this->wddm->skipResourceCleanup());
    EXPECT_NO_THROW(osContext->reInitializeContext());
    EXPECT_NO_THROW(osContext->ensureContextInitialized(false));
}
