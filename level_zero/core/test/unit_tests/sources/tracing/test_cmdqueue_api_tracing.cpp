/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandQueueCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.CommandQueue.pfnCreate =
        [](ze_context_handle_t hContext,
           ze_device_handle_t hDevice,
           const ze_command_queue_desc_t *desc,
           ze_command_queue_handle_t *phCommandQueue) { return ZE_RESULT_SUCCESS; };
    ze_command_queue_desc_t desc = {};
    ze_command_queue_handle_t commandQueue = {};

    prologCbs.CommandQueue.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.CommandQueue.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandQueueCreateTracing(nullptr, nullptr, &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandQueueDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.CommandQueue.pfnDestroy =
        [](ze_command_queue_handle_t hCommandQueue) { return ZE_RESULT_SUCCESS; };
    prologCbs.CommandQueue.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.CommandQueue.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandQueueDestroyTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandQueueExecuteCommandListsTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    uint32_t numCommandList = 0;
    ze_command_list_handle_t phCommandLists = {};
    ze_fence_handle_t hFence = nullptr;

    driver_ddiTable.core_ddiTable.CommandQueue.pfnExecuteCommandLists =
        [](ze_command_queue_handle_t hCommandQueue, uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists,
           ze_fence_handle_t hFence) { return ZE_RESULT_SUCCESS; };

    prologCbs.CommandQueue.pfnExecuteCommandListsCb = genericPrologCallbackPtr;
    epilogCbs.CommandQueue.pfnExecuteCommandListsCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandQueueExecuteCommandListsTracing(nullptr, numCommandList, &phCommandLists, hFence);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandQueueSynchronizeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.CommandQueue.pfnSynchronize =
        [](ze_command_queue_handle_t hCommandQueue, uint64_t timeout) { return ZE_RESULT_SUCCESS; };
    uint64_t timeout = 100;

    prologCbs.CommandQueue.pfnSynchronizeCb = genericPrologCallbackPtr;
    epilogCbs.CommandQueue.pfnSynchronizeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandQueueSynchronizeTracing(nullptr, timeout);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

} // namespace ult
} // namespace L0
