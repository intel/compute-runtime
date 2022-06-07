/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_allocation.h"
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
    using DebugSessionWindows::readAllocationDebugData;
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

TEST_F(DebugApiWindowsTest, givenDebugSessionInitializeCalledAndDebugAttachNtStatusIsFailedThenErrorUnknownReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    mockWddm->ntStatus = STATUS_UNSUCCESSFUL;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    auto result = session->initialize();

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
    EXPECT_FALSE(session->moduleDebugAreaCaptured);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_ATTACH_DEBUGGER]);
}

TEST_F(DebugApiWindowsTest, givenDebugSessionInitializeCalledAndDebugAttachEscapeReturnStatusIsFailedThenErrorUnsupportedFetaureReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    mockWddm->escapeReturnStatus = DBGUMD_RETURN_EU_DEBUG_NOT_SUPPORTED;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    auto result = session->initialize();

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_FALSE(session->moduleDebugAreaCaptured);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_ATTACH_DEBUGGER]);
}

TEST_F(DebugApiWindowsTest, givenDebugSessionInitializeCalledAndEventQueueIsEmptyThenAttachDebuggerAndReadEventEscapesAreInvokedAndErrorNotAvailableReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    // KMD event queue is empty
    mockWddm->numEvents = 0;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    auto result = session->initialize();

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    EXPECT_FALSE(session->moduleDebugAreaCaptured);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_ATTACH_DEBUGGER]);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_READ_EVENT]);
}

TEST_F(DebugApiWindowsTest, givenDebugSessionInitializeCalledAndEventQueueIsNotAmptyAndModuleDebugAreaIsNotCapturedThenAttachDebuggerAndReadEventEscapesAreInvokedAndErrorNotAvailableReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    mockWddm->numEvents = 1;
    mockWddm->eventQueue[0].readEventType = DBGUMD_READ_EVENT_CONTEXT_CREATE_DESTROY;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    auto result = session->initialize();

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    EXPECT_FALSE(session->moduleDebugAreaCaptured);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_ATTACH_DEBUGGER]);
    EXPECT_EQ(2, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_READ_EVENT]);
}

TEST_F(DebugApiWindowsTest, givenDebugSessionInitializeCalledAndEventQueueIsNotAmptyAndReadAllocationDataFailedThenAttachDebuggerAndReadEventAndReadAllocationDataEscapesAreInvokedAndResultUnknownIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    mockWddm->numEvents = 1;
    mockWddm->eventQueue[0].readEventType = DBGUMD_READ_EVENT_ALLOCATION_DATA_INFO;
    mockWddm->eventQueue[0].eventParamsBuffer.eventParamsBuffer.ReadAdditionalAllocDataParams.NumOfDebugData = 1;
    GFX_ALLOCATION_DEBUG_DATA_INFO *allocDebugDataInfo = reinterpret_cast<GFX_ALLOCATION_DEBUG_DATA_INFO *>(&mockWddm->eventQueue[2].eventParamsBuffer.eventParamsBuffer.ReadAdditionalAllocDataParams.DebugDataBufferPtr);
    allocDebugDataInfo->DataType = MODULE_HEAP_DEBUG_AREA;

    WddmAllocation::RegistrationData registrationData = {0x12345678, 0x1000};
    mockWddm->readAllocationDataOutParams.escapeReturnStatus = DBGUMD_RETURN_EU_DEBUG_NOT_SUPPORTED;
    mockWddm->readAllocationDataOutParams.outData = &registrationData;
    mockWddm->readAllocationDataOutParams.outDataSize = sizeof(registrationData);

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    auto result = session->initialize();

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_FALSE(session->moduleDebugAreaCaptured);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_ATTACH_DEBUGGER]);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_READ_EVENT]);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_READ_ALLOCATION_DATA]);
}

TEST_F(DebugApiWindowsTest, givenDebugSessionInitializeCalledAndEventQueueIsNotAmptyAndReadAllocationDataSucceedAndModuleDebugAreaNotCapturedThenAttachDebuggerAndReadEventAndReadAllocationDataEscapesAreInvokedAndResultUnavailableIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    mockWddm->numEvents = 1;
    mockWddm->eventQueue[0].readEventType = DBGUMD_READ_EVENT_ALLOCATION_DATA_INFO;
    mockWddm->eventQueue[0].eventParamsBuffer.eventParamsBuffer.ReadAdditionalAllocDataParams.NumOfDebugData = 1;
    GFX_ALLOCATION_DEBUG_DATA_INFO *allocDebugDataInfo = reinterpret_cast<GFX_ALLOCATION_DEBUG_DATA_INFO *>(&mockWddm->eventQueue[2].eventParamsBuffer.eventParamsBuffer.ReadAdditionalAllocDataParams.DebugDataBufferPtr);
    allocDebugDataInfo->DataType = ISA;

    WddmAllocation::RegistrationData registrationData = {0x12345678, 0x1000};
    mockWddm->readAllocationDataOutParams.outData = &registrationData;
    mockWddm->readAllocationDataOutParams.outDataSize = sizeof(registrationData);

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    auto result = session->initialize();

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    EXPECT_FALSE(session->moduleDebugAreaCaptured);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_ATTACH_DEBUGGER]);
    EXPECT_EQ(2, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_READ_EVENT]);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_READ_ALLOCATION_DATA]);
}

TEST_F(DebugApiWindowsTest, givenDebugSessionInitializeCalledAndEventQueueIsNotAmptyAndModuleDebugAreaIsCapturedThenAttachDebuggerAndReadEventEscapesAreInvokedAndResultSuccessReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    mockWddm->numEvents = 3;
    mockWddm->eventQueue[0].readEventType = DBGUMD_READ_EVENT_CONTEXT_CREATE_DESTROY;

    mockWddm->eventQueue[1].readEventType = DBGUMD_READ_EVENT_DEVICE_CREATE_DESTROY;

    mockWddm->eventQueue[2].readEventType = DBGUMD_READ_EVENT_ALLOCATION_DATA_INFO;
    mockWddm->eventQueue[2].eventParamsBuffer.eventParamsBuffer.ReadAdditionalAllocDataParams.NumOfDebugData = 1;
    GFX_ALLOCATION_DEBUG_DATA_INFO *allocDebugDataInfo = reinterpret_cast<GFX_ALLOCATION_DEBUG_DATA_INFO *>(&mockWddm->eventQueue[2].eventParamsBuffer.eventParamsBuffer.ReadAdditionalAllocDataParams.DebugDataBufferPtr);
    allocDebugDataInfo->DataType = MODULE_HEAP_DEBUG_AREA;

    WddmAllocation::RegistrationData registrationData = {0x12345678, 0x1000};
    mockWddm->readAllocationDataOutParams.outData = &registrationData;
    mockWddm->readAllocationDataOutParams.outDataSize = sizeof(registrationData);

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    auto result = session->initialize();

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(session->moduleDebugAreaCaptured);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_ATTACH_DEBUGGER]);
    EXPECT_EQ(3, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_READ_EVENT]);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_READ_ALLOCATION_DATA]);
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

    mockWddm->numEvents = 1;
    mockWddm->eventQueue[0].readEventType = DBGUMD_READ_EVENT_ALLOCATION_DATA_INFO;
    mockWddm->eventQueue[0].eventParamsBuffer.eventParamsBuffer.ReadAdditionalAllocDataParams.NumOfDebugData = 1;
    GFX_ALLOCATION_DEBUG_DATA_INFO *allocDebugDataInfo = reinterpret_cast<GFX_ALLOCATION_DEBUG_DATA_INFO *>(&mockWddm->eventQueue[0].eventParamsBuffer.eventParamsBuffer.ReadAdditionalAllocDataParams.DebugDataBufferPtr);
    allocDebugDataInfo->DataType = MODULE_HEAP_DEBUG_AREA;

    WddmAllocation::RegistrationData registrationData = {0x12345678, 0x1000};
    mockWddm->readAllocationDataOutParams.outData = &registrationData;
    mockWddm->readAllocationDataOutParams.outDataSize = sizeof(registrationData);

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    auto result = session->initialize();

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_ATTACH_DEBUGGER]);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_READ_EVENT]);
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_READ_ALLOCATION_DATA]);
    EXPECT_EQ(session->processId, config.pid);
    EXPECT_EQ(session->debugHandle, mockWddm->debugHandle);

    EXPECT_TRUE(session->closeConnection());
    EXPECT_EQ(1, mockWddm->dbgUmdEscapeActionCalled[DBGUMD_ACTION_DETACH_DEBUGGER]);
}

TEST_F(DebugApiWindowsTest, givenNtStatusFailedWhenReadAndHandleEventCalledThenResultUnknownIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    session->wddm = mockWddm;

    mockWddm->ntStatus = STATUS_UNSUCCESSFUL;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->readAndHandleEvent(100));
}

TEST_F(DebugApiWindowsTest, givenEscapeReturnStatusFailedWhenReadAndHandleEventCalledThenResultUnsupportedFeatureIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    session->wddm = mockWddm;

    mockWddm->numEvents = 1;
    mockWddm->eventQueue[0].escapeReturnStatus = DBGUMD_RETURN_EU_DEBUG_NOT_SUPPORTED;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->readAndHandleEvent(100));
}

TEST_F(DebugApiWindowsTest, givenUnsupportedEventTypeWhenReadAndHandleEventCalledThenResultUnsupportedFeatureIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    session->wddm = mockWddm;

    mockWddm->numEvents = 1;
    for (auto unsupportedEventType : {DBGUMD_READ_EVENT_MODULE_CREATE_NOTIFICATION, DBGUMD_READ_EVENT_EU_ATTN_BIT_SET}) {
        mockWddm->curEvent = 0;
        mockWddm->eventQueue[0].readEventType = unsupportedEventType;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->readAndHandleEvent(100));
    }
}

TEST_F(DebugApiWindowsTest, givenUnknownEventTypeWhenReadAndHandleEventCalledThenResultUnknownIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    session->wddm = mockWddm;

    mockWddm->numEvents = 1;
    mockWddm->eventQueue[0].readEventType = DBGUMD_READ_EVENT_MAX;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->readAndHandleEvent(100));
}

TEST_F(DebugApiWindowsTest, givenNtStatusFailedWhenReadAllocationDebugDataCalledThenResultUnknownIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    session->wddm = mockWddm;

    mockWddm->ntStatus = STATUS_UNSUCCESSFUL;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, session->readAllocationDebugData(1, 0x1234, nullptr, 0));
}

TEST_F(DebugApiWindowsTest, givenEscapeReturnStatusFailedWhenReadAllocationDebugDataCalledThenResultUnsupportedFeatureIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    session->wddm = mockWddm;

    mockWddm->readAllocationDataOutParams.escapeReturnStatus = DBGUMD_RETURN_EU_DEBUG_NOT_SUPPORTED;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->readAllocationDebugData(1, 0x1234, nullptr, 0));
}

TEST_F(DebugApiWindowsTest, givenDebugDataEventTypeWhenReadAndHandleEventCalledThenResultDebugDataIsSaved) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionWindows>(config, device);
    session->wddm = mockWddm;

    mockWddm->numEvents = 1;
    mockWddm->eventQueue[0].readEventType = DBGUMD_READ_EVENT_CREATE_DEBUG_DATA;
    mockWddm->eventQueue[0].eventParamsBuffer.eventParamsBuffer.ReadCreateDebugDataParams.DebugDataType = ELF_BINARY;
    mockWddm->eventQueue[0].eventParamsBuffer.eventParamsBuffer.ReadCreateDebugDataParams.DataBufferPtr = 0xa000;
    mockWddm->eventQueue[0].eventParamsBuffer.eventParamsBuffer.ReadCreateDebugDataParams.DataSize = 8;
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

    mockWddm->numEvents = 1;
    mockWddm->eventQueue[0].readEventType = DBGUMD_READ_EVENT_CONTEXT_CREATE_DESTROY;
    mockWddm->eventQueue[0].eventParamsBuffer.eventParamsBuffer.ContextCreateDestroyEventParams.hContextHandle = 0xa000;
    mockWddm->eventQueue[0].eventParamsBuffer.eventParamsBuffer.ContextCreateDestroyEventParams.IsCreated = 1;
    mockWddm->eventQueue[0].eventParamsBuffer.eventParamsBuffer.ContextCreateDestroyEventParams.IsSIPInstalled = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readAndHandleEvent(100));
    EXPECT_EQ(0, session->allContexts.size());

    mockWddm->curEvent = 0;
    mockWddm->eventQueue[0].eventParamsBuffer.eventParamsBuffer.ContextCreateDestroyEventParams.IsSIPInstalled = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readAndHandleEvent(100));
    EXPECT_EQ(1, session->allContexts.size());
    EXPECT_EQ(1, session->allContexts.count(0xa000));

    mockWddm->curEvent = 0;
    mockWddm->eventQueue[0].eventParamsBuffer.eventParamsBuffer.ContextCreateDestroyEventParams.IsCreated = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readAndHandleEvent(100));
    EXPECT_EQ(0, session->allContexts.size());
}

TEST(DebugSessionWindowsTest, whenTranslateNtStatusCalledThenCorrectZeResultReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, DebugSessionWindows::translateNtStatusToZeResult(STATUS_SUCCESS));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, DebugSessionWindows::translateNtStatusToZeResult(STATUS_INVALID_PARAMETER));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, DebugSessionWindows::translateNtStatusToZeResult(STATUS_UNSUCCESSFUL));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, DebugSessionWindows::translateNtStatusToZeResult(NTSTATUS(~0)));
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
