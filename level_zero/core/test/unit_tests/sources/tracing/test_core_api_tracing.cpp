/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

void onEnterCommandListAppendLaunchKernel(
    ze_command_list_append_launch_kernel_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    int a = 0;
    a++;
}
void onExitCommandListAppendLaunchKernel(
    ze_command_list_append_launch_kernel_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    int a = 0;
    a++;
}

void onEnterCommandListCreateWithUserData(
    ze_command_list_create_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(5, *val);
}
void onExitCommandListCreateWithUserData(
    ze_command_list_create_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(5, *val);
}

void onEnterCommandListCloseWithUserData(
    ze_command_list_close_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(5, *val);
}
void onExitCommandListCloseWithUserData(
    ze_command_list_close_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(5, *val);
}

void onEnterCommandListCloseWithUserDataRecursion(
    ze_command_list_close_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(5, *val);
    *val += 5;
}
void onExitCommandListCloseWithUserDataRecursion(
    ze_command_list_close_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(10, *val);
    *val += 5;
}

void onEnterCommandListCloseWithUserDataAndAllocateInstanceData(
    ze_command_list_close_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *userdata = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(5, *userdata);
    int *instanceData = new int;
    ppTracerInstanceUserData[0] = instanceData;
    *instanceData = 0x1234;
}
void onExitCommandListCloseWithUserDataAndReadInstanceData(
    ze_command_list_close_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *userdata = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(5, *userdata);
    ASSERT_NE(nullptr, ppTracerInstanceUserData);
    ASSERT_NE(nullptr, ppTracerInstanceUserData[0]);
    int *instanceData = static_cast<int *>(ppTracerInstanceUserData[0]);
    ASSERT_NE(nullptr, instanceData);
    if (nullptr == instanceData)
        return;
    int data = *instanceData;
    EXPECT_EQ(0x1234, data);
    delete instanceData;
}

void onEnterCommandListCloseWithoutUserDataAndAllocateInstanceData(
    ze_command_list_close_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_EQ(nullptr, pTracerUserData);
    int *instanceData = new int;
    ppTracerInstanceUserData[0] = instanceData;
    *instanceData = 0x1234;
}
void onExitCommandListCloseWithoutUserDataAndReadInstanceData(
    ze_command_list_close_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_EQ(nullptr, pTracerUserData);
    ASSERT_NE(nullptr, ppTracerInstanceUserData);
    ASSERT_NE(nullptr, ppTracerInstanceUserData[0]);
    int *instanceData = static_cast<int *>(ppTracerInstanceUserData[0]);
    ASSERT_NE(nullptr, instanceData);
    if (nullptr == instanceData)
        return;
    int data = *instanceData;
    EXPECT_EQ(0x1234, data);
    delete instanceData;
}

TEST(ZeApiTracingCoreTestsNoSetup, WhenCreateTracerAndNoZetInitThenReturnFailure) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    zet_tracer_exp_handle_t apiTracerHandle;
    zet_tracer_exp_desc_t tracerDesc = {};

    result = zetTracerExpCreate(nullptr, &tracerDesc, &apiTracerHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
}

TEST_F(ZeApiTracingCoreTests, WhenCreateTracerAndsetCallbacksAndEnableTracingAndDisableTracingAndDestroyTracerThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    zet_tracer_exp_handle_t apiTracerHandle;
    zet_tracer_exp_desc_t tracerDesc = {};

    result = zetTracerExpCreate(nullptr, &tracerDesc, &apiTracerHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, apiTracerHandle);

    zet_core_callbacks_t prologCbs = {};
    zet_core_callbacks_t epilogCbs = {};

    prologCbs.CommandList.pfnAppendLaunchKernelCb = onEnterCommandListAppendLaunchKernel;
    epilogCbs.CommandList.pfnAppendLaunchKernelCb = onExitCommandListAppendLaunchKernel;

    result = zetTracerExpSetPrologues(apiTracerHandle, &prologCbs);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetTracerExpSetEpilogues(apiTracerHandle, &epilogCbs);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetTracerExpSetEnabled(apiTracerHandle, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetTracerExpSetEnabled(apiTracerHandle, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetTracerExpDestroy(apiTracerHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(ZeApiTracingCoreTests, WhenCallingTracerWrapperWithOnePrologAndNoEpilogWithUserDataAndUserDataMatchingInPrologThenReturnSuccess) {
    MockCommandList commandList;
    ze_result_t result = ZE_RESULT_SUCCESS;
    int userData = 5;
    ze_command_list_close_params_t tracerParams;
    zet_core_callbacks_t prologCbs = {};

    prologCbs.CommandList.pfnCloseCb = onEnterCommandListCloseWithUserData;

    ze_command_list_handle_t commandListHandle = commandList.toHandle();
    tracerParams.phCommandList = &commandListHandle;

    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> prologCallbacks;
    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> epilogCallbacks;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> prologCallback;
    prologCallback.current_api_callback = prologCbs.CommandList.pfnCloseCb;
    prologCallback.pUserData = &userData;
    prologCallbacks.push_back(prologCallback);
    ze_pfnCommandListCloseCb_t apiOrdinal = {};

    result = apiTracerWrapperImp(zeCommandListClose, &tracerParams, apiOrdinal, prologCallbacks, epilogCallbacks, *tracerParams.phCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(ZeApiTracingCoreTests, WhenCallingTracerWrapperWithOneSetOfPrologEpilogsWithUserDataAndUserDataMatchingInPrologAndEpilogThenReturnSuccess) {
    MockCommandList commandList;
    ze_result_t result = ZE_RESULT_SUCCESS;
    int userData = 5;
    ze_command_list_close_params_t tracerParams;
    zet_core_callbacks_t prologCbs = {};
    zet_core_callbacks_t epilogCbs = {};

    prologCbs.CommandList.pfnCloseCb = onEnterCommandListCloseWithUserData;
    epilogCbs.CommandList.pfnCloseCb = onExitCommandListCloseWithUserData;

    ze_command_list_handle_t commandListHandle = commandList.toHandle();
    tracerParams.phCommandList = &commandListHandle;

    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> prologCallbacks;
    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> epilogCallbacks;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> prologCallback;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> epilogCallback;
    prologCallback.current_api_callback = prologCbs.CommandList.pfnCloseCb;
    epilogCallback.current_api_callback = epilogCbs.CommandList.pfnCloseCb;
    prologCallback.pUserData = &userData;
    epilogCallback.pUserData = &userData;
    prologCallbacks.push_back(prologCallback);
    epilogCallbacks.push_back(epilogCallback);
    ze_pfnCommandListCloseCb_t apiOrdinal = {};

    result = apiTracerWrapperImp(zeCommandListClose, &tracerParams, apiOrdinal, prologCallbacks, epilogCallbacks, *tracerParams.phCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(ZeApiTracingCoreTests, WhenCallingTracerWrapperWithOneSetOfPrologEpilogsWithUserDataAndInstanceDataUserDataMatchingInPrologAndEpilogThenReturnSuccess) {
    MockCommandList commandList;
    ze_result_t result = ZE_RESULT_SUCCESS;
    int userData = 5;
    ze_command_list_close_params_t tracerParams;
    zet_core_callbacks_t prologCbs = {};
    zet_core_callbacks_t epilogCbs = {};

    prologCbs.CommandList.pfnCloseCb = onEnterCommandListCloseWithUserDataAndAllocateInstanceData;
    epilogCbs.CommandList.pfnCloseCb = onExitCommandListCloseWithUserDataAndReadInstanceData;

    ze_command_list_handle_t commandListHandle = commandList.toHandle();
    tracerParams.phCommandList = &commandListHandle;

    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> prologCallbacks;
    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> epilogCallbacks;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> prologCallback;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> epilogCallback;
    prologCallback.current_api_callback = prologCbs.CommandList.pfnCloseCb;
    epilogCallback.current_api_callback = epilogCbs.CommandList.pfnCloseCb;
    prologCallback.pUserData = &userData;
    epilogCallback.pUserData = &userData;
    prologCallbacks.push_back(prologCallback);
    epilogCallbacks.push_back(epilogCallback);
    ze_pfnCommandListCloseCb_t apiOrdinal = {};

    result = apiTracerWrapperImp(zeCommandListClose, &tracerParams, apiOrdinal, prologCallbacks, epilogCallbacks, *tracerParams.phCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(ZeApiTracingCoreTests, WhenCallingTracerWrapperWithOneSetOfPrologEpilogsWithInstanceDataThenReturnSuccess) {
    MockCommandList commandList;
    ze_result_t result = ZE_RESULT_SUCCESS;
    ze_command_list_close_params_t tracerParams;
    zet_core_callbacks_t prologCbs = {};
    zet_core_callbacks_t epilogCbs = {};

    prologCbs.CommandList.pfnCloseCb = onEnterCommandListCloseWithoutUserDataAndAllocateInstanceData;
    epilogCbs.CommandList.pfnCloseCb = onExitCommandListCloseWithoutUserDataAndReadInstanceData;

    ze_command_list_handle_t commandListHandle = commandList.toHandle();
    tracerParams.phCommandList = &commandListHandle;

    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> prologCallbacks;
    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> epilogCallbacks;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> prologCallback;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> epilogCallback;
    prologCallback.current_api_callback = prologCbs.CommandList.pfnCloseCb;
    epilogCallback.current_api_callback = epilogCbs.CommandList.pfnCloseCb;
    prologCallback.pUserData = nullptr;
    epilogCallback.pUserData = nullptr;
    prologCallbacks.push_back(prologCallback);
    epilogCallbacks.push_back(epilogCallback);
    ze_pfnCommandListCloseCb_t apiOrdinal = {};

    result = apiTracerWrapperImp(zeCommandListClose, &tracerParams, apiOrdinal, prologCallbacks, epilogCallbacks, *tracerParams.phCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(ZeApiTracingCoreTests, WhenCallingTracerWrapperWithOneSetOfPrologEpilogsWithRecursionHandledThenSuccessIsReturned) {
    MockCommandList commandList;
    ze_result_t result = ZE_RESULT_SUCCESS;
    int userData = 5;
    ze_command_list_close_params_t tracerParams;
    zet_core_callbacks_t prologCbs = {};
    zet_core_callbacks_t epilogCbs = {};

    prologCbs.CommandList.pfnCloseCb = onEnterCommandListCloseWithUserDataRecursion;
    epilogCbs.CommandList.pfnCloseCb = onExitCommandListCloseWithUserDataRecursion;

    ze_command_list_handle_t commandListHandle = commandList.toHandle();
    tracerParams.phCommandList = &commandListHandle;

    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> prologCallbacks;
    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> epilogCallbacks;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> prologCallback;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> epilogCallback;
    prologCallback.current_api_callback = prologCbs.CommandList.pfnCloseCb;
    epilogCallback.current_api_callback = epilogCbs.CommandList.pfnCloseCb;
    prologCallback.pUserData = &userData;
    epilogCallback.pUserData = &userData;
    prologCallbacks.push_back(prologCallback);
    epilogCallbacks.push_back(epilogCallback);
    ze_pfnCommandListCloseCb_t apiOrdinal = {};

    result = callHandleTracerRecursion(zeCommandListClose, commandListHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    result = apiTracerWrapperImp(zeCommandListClose, &tracerParams, apiOrdinal, prologCallbacks, epilogCallbacks, *tracerParams.phCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = callHandleTracerRecursion(zeCommandListClose, commandListHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    result = callHandleTracerRecursion(zeCommandListClose, commandListHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    L0::tracingInProgress = 0;
}

} // namespace ult
} // namespace L0
