/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnCreate =
        [](ze_context_handle_t hContext,
           ze_device_handle_t hDevice,
           const ze_command_list_desc_t *desc,
           ze_command_list_handle_t *phCommandList) { return ZE_RESULT_SUCCESS; };
    ze_command_list_desc_t desc = {};
    ze_command_list_handle_t commandList = {};

    prologCbs.CommandList.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListCreateTracing(nullptr, nullptr, &desc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListCreateImmediateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnCreateImmediate =
        [](ze_context_handle_t hContext,
           ze_device_handle_t hDevice,
           const ze_command_queue_desc_t *desc,
           ze_command_list_handle_t *phCommandList) { return ZE_RESULT_SUCCESS; };
    ze_command_queue_desc_t desc = {};
    ze_command_list_handle_t commandList = {};

    prologCbs.CommandList.pfnCreateImmediateCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnCreateImmediateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListCreateImmediateTracing(nullptr, nullptr, &desc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnDestroy =
        [](ze_command_list_handle_t hCommandList) { return ZE_RESULT_SUCCESS; };
    prologCbs.CommandList.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListDestroyTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListResetTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnReset =
        [](ze_command_list_handle_t hCommandList) { return ZE_RESULT_SUCCESS; };
    prologCbs.CommandList.pfnResetCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnResetCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListResetTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendMemoryPrefetchTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryPrefetch =
        [](ze_command_list_handle_t hCommandList, const void *ptr, size_t size) { return ZE_RESULT_SUCCESS; };
    prologCbs.CommandList.pfnAppendMemoryPrefetchCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendMemoryPrefetchCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendMemoryPrefetchTracing(nullptr, nullptr, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListCloseTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnClose =
        [](ze_command_list_handle_t hCommandList) { return ZE_RESULT_SUCCESS; };
    prologCbs.CommandList.pfnCloseCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnCloseCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListCloseTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendQueryKernelTimestampsTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    driver_ddiTable.core_ddiTable.CommandList.pfnAppendQueryKernelTimestamps =
        [](ze_command_list_handle_t hCommandList,
           uint32_t numEvents,
           ze_event_handle_t *phEvents,
           void *dstptr,
           const size_t *pOffsets,
           ze_event_handle_t hSignalEvent,
           uint32_t numWaitEvents,
           ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };

    prologCbs.CommandList.pfnAppendQueryKernelTimestampsCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendQueryKernelTimestampsCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendQueryKernelTimestampsTracing(nullptr, 1U, nullptr, nullptr, nullptr, nullptr, 1U, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendWriteGlobalTimestampTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendWriteGlobalTimestamp =
        [](ze_command_list_handle_t hCommandList,
           uint64_t *dstptr,
           ze_event_handle_t hSignalEvent,
           uint32_t numWaitEvents,
           ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };

    prologCbs.CommandList.pfnAppendWriteGlobalTimestampCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendWriteGlobalTimestampCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendWriteGlobalTimestampTracing(nullptr, nullptr, nullptr, 1U, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

} // namespace ult
} // namespace L0
