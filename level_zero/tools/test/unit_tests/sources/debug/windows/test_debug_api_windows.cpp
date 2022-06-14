/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/windows/mock_wddm_eudebug.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/debug/windows/debug_session.h"

namespace L0 {
namespace ult {

struct MockDebugSessionWindows : DebugSessionWindows {
    using DebugSessionWindows::allContexts;
    using DebugSessionWindows::allElfs;
    using DebugSessionWindows::asyncThread;
    using DebugSessionWindows::closeAsyncThread;
    using DebugSessionWindows::debugHandle;
    using DebugSessionWindows::initialize;
    using DebugSessionWindows::moduleDebugAreaCaptured;
    using DebugSessionWindows::processId;
    using DebugSessionWindows::readAndHandleEvent;
    using DebugSessionWindows::startAsyncThread;
    using DebugSessionWindows::wddm;

    MockDebugSessionWindows(const zet_debug_config_t &config, L0::Device *device) : DebugSessionWindows(config, device) {}

    ze_result_t initialize() override {
        if (resultInitialize != ZE_RESULT_FORCE_UINT32) {
            return resultInitialize;
        }
        return DebugSessionWindows::initialize();
    }

    ze_result_t readAndHandleEvent(uint64_t timeoutMs) override {
        if (resultReadAndHandleEvent != ZE_RESULT_FORCE_UINT32) {
            return resultReadAndHandleEvent;
        }
        return DebugSessionWindows::readAndHandleEvent(timeoutMs);
    }

    ze_result_t resultInitialize = ZE_RESULT_FORCE_UINT32;
    ze_result_t resultReadAndHandleEvent = ZE_RESULT_FORCE_UINT32;
    static constexpr uint64_t mockDebugHandle = 1;
};

struct DebugApiWindowsFixture : public DeviceFixture {
    void SetUp() {
        DeviceFixture::SetUp();
        mockWddm = new WddmEuDebugInterfaceMock(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }

    WddmEuDebugInterfaceMock *mockWddm = nullptr;
};

using DebugApiWindowsTest = Test<DebugApiWindowsFixture>;

TEST_F(DebugApiWindowsTest, givenDebugAttachIsNotAvailableWhenGetDebugPropertiesCalledThenNoFlagIsReturned) {
    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    mockWddm->debugAttachAvailable = false;
    auto result = zetDeviceGetDebugProperties(device->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, debugProperties.flags);
}

using isDebugSupportedProduct = IsWithinProducts<IGFX_DG1, IGFX_PVC>;
HWTEST2_F(DebugApiWindowsTest, givenDebugAttachAvailableWhenGetDebugPropertiesCalledThenCorrectFlagIsReturned, isDebugSupportedProduct) {
    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    auto result = zetDeviceGetDebugProperties(device->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH, debugProperties.flags);
}

TEST_F(DebugApiWindowsTest, givenDebugAttachIsNotAvailableWhenDebugSessionCreatedThenNullptrAndResultUnsupportedFeatureAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession = nullptr;

    mockWddm->debugAttachAvailable = false;
    auto result = zetDebugAttach(device->toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(nullptr, debugSession);
}

TEST_F(DebugApiWindowsTest, givenDebugAttachAvailableAndInitializationFailedWhenDebugSessionCreatedThenNullptrAndErrorStatusAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x0; // debugSession->initialize() will fail

    zet_debug_session_handle_t debugSession = nullptr;
    auto result = zetDebugAttach(device->toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
    EXPECT_EQ(nullptr, debugSession);
}

TEST_F(DebugApiWindowsTest, givenDebugSessionInitializeCalledAndModuleDebugAreaNotCapturedThenAttachDebuggerAndReadEventEscapesAreInvokedAndErrorNotAvailableReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    // KMD event queue is empty
    mockWddm->readEventOutParams.escapeReturnStatus = DBGUMD_RETURN_READ_EVENT_TIMEOUT_EXPIRED;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    auto result = session->initialize();

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    EXPECT_FALSE(session->moduleDebugAreaCaptured);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_ATTACH_DEBUGGER]);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_READ_EVENT]);
}

TEST_F(DebugApiWindowsTest, givenDebugSessionInitializeCalledAndModuleDebugAreaCapturedThenAttachDebuggerAndReadEventEscapesAreInvokedAndResultSuccessReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    GFX_ALLOCATION_DEBUG_DATA_INFO allocDebugDataInfo = {0};
    allocDebugDataInfo.DataType = MODULE_HEAP_DEBUG_AREA;
    mockWddm->readEventOutParams.readEventType = DBGUMD_READ_EVENT_ALLOCATION_DATA_INFO;
    mockWddm->readEventOutParams.eventParamsBuffer.ReadAdditionalAllocDataParams.NumOfDebugData = 1;
    mockWddm->readEventOutParams.eventParamsBuffer.ReadAdditionalAllocDataParams.DebugDataBufferPtr = reinterpret_cast<uint64_t>(&allocDebugDataInfo);

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    auto result = session->initialize();

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(session->moduleDebugAreaCaptured);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_ATTACH_DEBUGGER]);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_READ_EVENT]);
    EXPECT_EQ(session->processId, config.pid);
    EXPECT_EQ(session->debugHandle, mockWddm->debugHandle);
}

TEST_F(DebugApiWindowsTest, givenDebugSessionCloseConnectionCalledWithoutInitializationThenFalseIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    EXPECT_FALSE(session->closeConnection());
}

TEST_F(DebugApiWindowsTest, givenDebugSessionInitializedAndCloseConnectionCalledThenDebuggerDetachEscapeIsInvoked) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    GFX_ALLOCATION_DEBUG_DATA_INFO allocDebugDataInfo = {0};
    allocDebugDataInfo.DataType = MODULE_HEAP_DEBUG_AREA;
    mockWddm->readEventOutParams.readEventType = DBGUMD_READ_EVENT_ALLOCATION_DATA_INFO;
    mockWddm->readEventOutParams.eventParamsBuffer.ReadAdditionalAllocDataParams.NumOfDebugData = 1;
    mockWddm->readEventOutParams.eventParamsBuffer.ReadAdditionalAllocDataParams.DebugDataBufferPtr = reinterpret_cast<uint64_t>(&allocDebugDataInfo);

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    auto result = session->initialize();

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_ATTACH_DEBUGGER]);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_READ_EVENT]);
    EXPECT_EQ(session->processId, config.pid);
    EXPECT_EQ(session->debugHandle, mockWddm->debugHandle);

    EXPECT_TRUE(session->closeConnection());
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_DETACH_DEBUGGER]);
}

TEST_F(DebugApiWindowsTest, givenUnsupportedEventTypeWhenReadAndHandleEventCalledThenResultUnsupportedFeatureIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    session->wddm = mockWddm;

    for (auto unsupportedEventType : {DBGUMD_READ_EVENT_MODULE_CREATE_NOTIFICATION,
                                      DBGUMD_READ_EVENT_EU_ATTN_BIT_SET,
                                      DBGUMD_READ_EVENT_DEVICE_CREATE_DESTROY}) {
        mockWddm->readEventOutParams.readEventType = unsupportedEventType;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->readAndHandleEvent(100));
    }
}

TEST_F(DebugApiWindowsTest, givenUnknownEventTypeWhenReadAndHandleEventCalledThenResultUnknownIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    session->wddm = mockWddm;
    mockWddm->readEventOutParams.readEventType = DBGUMD_READ_EVENT_MAX;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->readAndHandleEvent(100));
}

TEST_F(DebugApiWindowsTest, givenDebugDataEventTypeWhenReadAndHandleEventCalledThenResultDebugDataIsSaved) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    session->wddm = mockWddm;

    mockWddm->readEventOutParams.readEventType = DBGUMD_READ_EVENT_CREATE_DEBUG_DATA;
    mockWddm->readEventOutParams.eventParamsBuffer.ReadCreateDebugDataParams.DebugDataType = ELF_BINARY;
    mockWddm->readEventOutParams.eventParamsBuffer.ReadCreateDebugDataParams.DataBufferPtr = 0xa000;
    mockWddm->readEventOutParams.eventParamsBuffer.ReadCreateDebugDataParams.DataSize = 8;
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readAndHandleEvent(100));
    EXPECT_EQ(1, session->allElfs.size());
    auto elf = session->allElfs[0];
    EXPECT_EQ(elf.startVA, 0xa000);
    EXPECT_EQ(elf.endVA, 0xa008);
}

TEST_F(DebugApiWindowsTest, givenContextCreateEventTypeWhenReadAndHandleEventCalledThenAllContextsIsSetCorrectly) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    session->wddm = mockWddm;

    mockWddm->readEventOutParams.readEventType = DBGUMD_READ_EVENT_CONTEXT_CREATE_DESTROY;
    mockWddm->readEventOutParams.eventParamsBuffer.ContextCreateDestroyEventParams.hContextHandle = 0xa000;
    mockWddm->readEventOutParams.eventParamsBuffer.ContextCreateDestroyEventParams.IsCreated = 1;
    mockWddm->readEventOutParams.eventParamsBuffer.ContextCreateDestroyEventParams.IsSIPInstalled = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readAndHandleEvent(100));
    EXPECT_EQ(0, session->allContexts.size());

    mockWddm->readEventOutParams.eventParamsBuffer.ContextCreateDestroyEventParams.IsSIPInstalled = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readAndHandleEvent(100));
    EXPECT_EQ(1, session->allContexts.size());
    EXPECT_EQ(1, session->allContexts.count(0xa000));

    mockWddm->readEventOutParams.eventParamsBuffer.ContextCreateDestroyEventParams.IsCreated = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readAndHandleEvent(100));
    EXPECT_EQ(0, session->allContexts.size());
}

TEST(DebugSessionWindowsTest, whenTranslateEscapeErrorStatusCalledThenCorrectZeResultReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, DebugSessionWindows::translateEscapeReturnStatusToZeResult(DBGUMD_RETURN_ESCAPE_SUCCESS));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, DebugSessionWindows::translateEscapeReturnStatusToZeResult(DBGUMD_RETURN_DEBUGGER_ATTACH_DEVICE_BUSY));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, DebugSessionWindows::translateEscapeReturnStatusToZeResult(DBGUMD_RETURN_READ_EVENT_TIMEOUT_EXPIRED));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, DebugSessionWindows::translateEscapeReturnStatusToZeResult(DBGUMD_RETURN_INVALID_ARGS));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, DebugSessionWindows::translateEscapeReturnStatusToZeResult(DBGUMD_RETURN_NOT_VALID_PROCESS));
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, DebugSessionWindows::translateEscapeReturnStatusToZeResult(DBGUMD_RETURN_PERMISSION_DENIED));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, DebugSessionWindows::translateEscapeReturnStatusToZeResult(DBGUMD_RETURN_EU_DEBUG_NOT_SUPPORTED));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, DebugSessionWindows::translateEscapeReturnStatusToZeResult(DBGUMD_RETURN_TYPE_MAX));
}

using DebugApiWindowsAsyncThreadTest = Test<DebugApiWindowsFixture>;

TEST_F(DebugApiWindowsAsyncThreadTest, GivenDebugSessionWhenStartingAndClosingAsyncThreadThenThreadIsStartedAndFinishes) {
    auto session = std::make_unique<MockDebugSessionWindows>(zet_debug_config_t{0x1234}, device);
    ASSERT_NE(nullptr, session);
    session->wddm = mockWddm;
    session->startAsyncThread();

    EXPECT_TRUE(session->asyncThread.threadActive);
    EXPECT_FALSE(session->asyncThread.threadFinished);

    session->closeAsyncThread();

    EXPECT_FALSE(session->asyncThread.threadActive);
    EXPECT_TRUE(session->asyncThread.threadFinished);
}

TEST_F(DebugApiWindowsAsyncThreadTest, GivenDebugSessionWithAsyncThreadWhenClosingConnectionThenAsyncThreadIsTerminated) {
    auto session = std::make_unique<MockDebugSessionWindows>(zet_debug_config_t{0x1234}, device);
    ASSERT_NE(nullptr, session);
    session->wddm = mockWddm;
    session->debugHandle = MockDebugSessionWindows::mockDebugHandle;

    session->startAsyncThread();

    EXPECT_TRUE(session->asyncThread.threadActive);
    EXPECT_FALSE(session->asyncThread.threadFinished);

    session->closeConnection();

    EXPECT_FALSE(session->asyncThread.threadActive);
    EXPECT_TRUE(session->asyncThread.threadFinished);
}

} // namespace ult
} // namespace L0
