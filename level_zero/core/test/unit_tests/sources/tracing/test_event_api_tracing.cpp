/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingEventCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Event.pfnCreate =
        [](ze_event_pool_handle_t hEventPool, const ze_event_desc_t *desc, ze_event_handle_t *phEvent) { return ZE_RESULT_SUCCESS; };
    ze_event_handle_t event = {};
    ze_event_desc_t desc = {};

    prologCbs.Event.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.Event.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeEventCreateTracing(nullptr, &desc, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingEventDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Event.pfnDestroy =
        [](ze_event_handle_t hEvent) { return ZE_RESULT_SUCCESS; };

    prologCbs.Event.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.Event.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeEventDestroyTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingEventHostSignalTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Event.pfnHostSignal =
        [](ze_event_handle_t hEvent) { return ZE_RESULT_SUCCESS; };
    prologCbs.Event.pfnHostSignalCb = genericPrologCallbackPtr;
    epilogCbs.Event.pfnHostSignalCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeEventHostSignalTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingEventHostSynchronizeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Event.pfnHostSynchronize =
        [](ze_event_handle_t hEvent, uint64_t timeout) { return ZE_RESULT_SUCCESS; };
    prologCbs.Event.pfnHostSynchronizeCb = genericPrologCallbackPtr;
    epilogCbs.Event.pfnHostSynchronizeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeEventHostSynchronizeTracing(nullptr, 1U);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingEventQueryStatusTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Event.pfnQueryStatus =
        [](ze_event_handle_t hEvent) { return ZE_RESULT_SUCCESS; };
    prologCbs.Event.pfnQueryStatusCb = genericPrologCallbackPtr;
    epilogCbs.Event.pfnQueryStatusCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeEventQueryStatusTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingEventHostResetTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Event.pfnHostReset =
        [](ze_event_handle_t hEvent) { return ZE_RESULT_SUCCESS; };
    prologCbs.Event.pfnHostResetCb = genericPrologCallbackPtr;
    epilogCbs.Event.pfnHostResetCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeEventHostResetTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingEventPoolCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.EventPool.pfnCreate =
        [](ze_context_handle_t hContext,
           const ze_event_pool_desc_t *desc,
           uint32_t numDevices,
           ze_device_handle_t *phDevices,
           ze_event_pool_handle_t *phEventPool) { return ZE_RESULT_SUCCESS; };
    prologCbs.EventPool.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.EventPool.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolCreateTracing(nullptr, nullptr, 1U, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingEventPoolDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.EventPool.pfnDestroy =
        [](ze_event_pool_handle_t hEventPool) { return ZE_RESULT_SUCCESS; };
    prologCbs.EventPool.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.EventPool.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolDestroyTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingEventPoolGetIpcHandleTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.EventPool.pfnGetIpcHandle =
        [](ze_event_pool_handle_t hEventPool, ze_ipc_event_pool_handle_t *phIpc) { return ZE_RESULT_SUCCESS; };
    ze_ipc_event_pool_handle_t phIpc;

    prologCbs.EventPool.pfnGetIpcHandleCb = genericPrologCallbackPtr;
    epilogCbs.EventPool.pfnGetIpcHandleCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolGetIpcHandleTracing(nullptr, &phIpc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingEventPoolOpenIpcHandleTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.EventPool.pfnOpenIpcHandle =
        [](ze_context_handle_t hDriver,
           ze_ipc_event_pool_handle_t hIpc,
           ze_event_pool_handle_t *phEventPool) { return ZE_RESULT_SUCCESS; };
    ze_ipc_event_pool_handle_t hIpc = {};
    ze_event_pool_handle_t phEventPool;

    prologCbs.EventPool.pfnOpenIpcHandleCb = genericPrologCallbackPtr;
    epilogCbs.EventPool.pfnOpenIpcHandleCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolOpenIpcHandleTracing(nullptr, hIpc, &phEventPool);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingEventPoolCloseIpcHandleTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.EventPool.pfnCloseIpcHandle =
        [](ze_event_pool_handle_t hEventPool) { return ZE_RESULT_SUCCESS; };
    prologCbs.EventPool.pfnCloseIpcHandleCb = genericPrologCallbackPtr;
    epilogCbs.EventPool.pfnCloseIpcHandleCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolCloseIpcHandleTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

// Command List API with Events

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendSignalEventTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendSignalEvent =
        [](ze_command_list_handle_t hCommandList, ze_event_handle_t hEvent) { return ZE_RESULT_SUCCESS; };
    prologCbs.CommandList.pfnAppendSignalEventCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendSignalEventCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendSignalEventTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendWaitOnEventsTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendWaitOnEvents =
        [](ze_command_list_handle_t hCommandList, uint32_t numEvents, ze_event_handle_t *phEvents) { return ZE_RESULT_SUCCESS; };

    ze_event_handle_t phEvents = {};

    prologCbs.CommandList.pfnAppendWaitOnEventsCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendWaitOnEventsCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendWaitOnEventsTracing(nullptr, 1, &phEvents);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendEventResetTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendEventReset =
        [](ze_command_list_handle_t hCommandList, ze_event_handle_t hEvent) { return ZE_RESULT_SUCCESS; };

    prologCbs.CommandList.pfnAppendEventResetCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendEventResetCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendEventResetTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingEventQueryKernelTimestampTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Event.pfnQueryKernelTimestamp =
        [](ze_event_handle_t hEvent, ze_kernel_timestamp_result_t *dstptr) { return ZE_RESULT_SUCCESS; };

    prologCbs.Event.pfnQueryKernelTimestampCb = genericPrologCallbackPtr;
    epilogCbs.Event.pfnQueryKernelTimestampCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeEventQueryKernelTimestampTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

// See test_event_api_multi_tracing.cpp for more tests

} // namespace ult
} // namespace L0
