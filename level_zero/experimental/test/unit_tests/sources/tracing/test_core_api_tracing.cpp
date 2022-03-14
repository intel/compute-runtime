/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

void OnEnterCommandListAppendLaunchFunction(
    ze_command_list_append_launch_kernel_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    int a = 0;
    a++;
}
void OnExitCommandListAppendLaunchFunction(
    ze_command_list_append_launch_kernel_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    int a = 0;
    a++;
}

void OnEnterCommandListCreateWithUserData(
    ze_command_list_create_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(5, *val);
}
void OnExitCommandListCreateWithUserData(
    ze_command_list_create_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(5, *val);
}

void OnEnterCommandListCloseWithUserData(
    ze_command_list_close_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(5, *val);
}
void OnExitCommandListCloseWithUserData(
    ze_command_list_close_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(5, *val);
}

void OnEnterCommandListCloseWithUserDataRecursion(
    ze_command_list_close_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(5, *val);
    *val += 5;
}
void OnExitCommandListCloseWithUserDataRecursion(
    ze_command_list_close_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(10, *val);
    *val += 5;
}

void OnEnterCommandListCloseWithUserDataAndAllocateInstanceData(
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
void OnExitCommandListCloseWithUserDataAndReadInstanceData(
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

void OnEnterCommandListCloseWithoutUserDataAndAllocateInstanceData(
    ze_command_list_close_params_t *params,
    ze_result_t result,
    void *pTracerUserData,
    void **ppTracerInstanceUserData) {
    ASSERT_EQ(nullptr, pTracerUserData);
    int *instanceData = new int;
    ppTracerInstanceUserData[0] = instanceData;
    *instanceData = 0x1234;
}
void OnExitCommandListCloseWithoutUserDataAndReadInstanceData(
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

TEST(zeAPITracingCoreTestsNoSetup, WhenCreateTracerAndNoZetInitThenReturnFailure) {
    ze_result_t result;

    zet_tracer_exp_handle_t APITracerHandle;
    zet_tracer_exp_desc_t tracer_desc = {};

    result = zetTracerExpCreate(nullptr, &tracer_desc, &APITracerHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
}

TEST_F(zeAPITracingCoreTests, WhenCreateTracerAndsetCallbacksAndEnableTracingAndDisableTracingAndDestroyTracerThenReturnSuccess) {
    ze_result_t result;

    zet_tracer_exp_handle_t APITracerHandle;
    zet_tracer_exp_desc_t tracer_desc = {};

    result = zetTracerExpCreate(nullptr, &tracer_desc, &APITracerHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, APITracerHandle);

    zet_core_callbacks_t prologCbs = {};
    zet_core_callbacks_t epilogCbs = {};

    prologCbs.CommandList.pfnAppendLaunchKernelCb = OnEnterCommandListAppendLaunchFunction;
    epilogCbs.CommandList.pfnAppendLaunchKernelCb = OnExitCommandListAppendLaunchFunction;

    result = zetTracerExpSetPrologues(APITracerHandle, &prologCbs);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetTracerExpSetEpilogues(APITracerHandle, &epilogCbs);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetTracerExpSetEnabled(APITracerHandle, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetTracerExpSetEnabled(APITracerHandle, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetTracerExpDestroy(APITracerHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(zeAPITracingCoreTests, WhenCallingTracerWrapperWithOnePrologAndNoEpilogWithUserDataAndUserDataMatchingInPrologThenReturnSuccess) {
    MockCommandList commandList;
    ze_result_t result;
    int user_data = 5;
    ze_command_list_close_params_t tracerParams;
    zet_core_callbacks_t prologCbs = {};

    prologCbs.CommandList.pfnCloseCb = OnEnterCommandListCloseWithUserData;

    ze_command_list_handle_t command_list_handle = commandList.toHandle();
    tracerParams.phCommandList = &command_list_handle;

    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> prologCallbacks;
    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> epilogCallbacks;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> prologCallback;
    prologCallback.current_api_callback = prologCbs.CommandList.pfnCloseCb;
    prologCallback.pUserData = &user_data;
    prologCallbacks.push_back(prologCallback);
    ze_pfnCommandListCloseCb_t apiOrdinal = {};

    result = APITracerWrapperImp(zeCommandListClose, &tracerParams, apiOrdinal, prologCallbacks, epilogCallbacks, *tracerParams.phCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(zeAPITracingCoreTests, WhenCallingTracerWrapperWithOneSetOfPrologEpilogsWithUserDataAndUserDataMatchingInPrologAndEpilogThenReturnSuccess) {
    MockCommandList commandList;
    ze_result_t result;
    int user_data = 5;
    ze_command_list_close_params_t tracerParams;
    zet_core_callbacks_t prologCbs = {};
    zet_core_callbacks_t epilogCbs = {};

    prologCbs.CommandList.pfnCloseCb = OnEnterCommandListCloseWithUserData;
    epilogCbs.CommandList.pfnCloseCb = OnExitCommandListCloseWithUserData;

    ze_command_list_handle_t command_list_handle = commandList.toHandle();
    tracerParams.phCommandList = &command_list_handle;

    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> prologCallbacks;
    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> epilogCallbacks;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> prologCallback;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> epilogCallback;
    prologCallback.current_api_callback = prologCbs.CommandList.pfnCloseCb;
    epilogCallback.current_api_callback = epilogCbs.CommandList.pfnCloseCb;
    prologCallback.pUserData = &user_data;
    epilogCallback.pUserData = &user_data;
    prologCallbacks.push_back(prologCallback);
    epilogCallbacks.push_back(epilogCallback);
    ze_pfnCommandListCloseCb_t apiOrdinal = {};

    result = APITracerWrapperImp(zeCommandListClose, &tracerParams, apiOrdinal, prologCallbacks, epilogCallbacks, *tracerParams.phCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(zeAPITracingCoreTests, WhenCallingTracerWrapperWithOneSetOfPrologEpilogsWithUserDataAndInstanceDataUserDataMatchingInPrologAndEpilogThenReturnSuccess) {
    MockCommandList commandList;
    ze_result_t result;
    int user_data = 5;
    ze_command_list_close_params_t tracerParams;
    zet_core_callbacks_t prologCbs = {};
    zet_core_callbacks_t epilogCbs = {};

    prologCbs.CommandList.pfnCloseCb = OnEnterCommandListCloseWithUserDataAndAllocateInstanceData;
    epilogCbs.CommandList.pfnCloseCb = OnExitCommandListCloseWithUserDataAndReadInstanceData;

    ze_command_list_handle_t command_list_handle = commandList.toHandle();
    tracerParams.phCommandList = &command_list_handle;

    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> prologCallbacks;
    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> epilogCallbacks;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> prologCallback;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> epilogCallback;
    prologCallback.current_api_callback = prologCbs.CommandList.pfnCloseCb;
    epilogCallback.current_api_callback = epilogCbs.CommandList.pfnCloseCb;
    prologCallback.pUserData = &user_data;
    epilogCallback.pUserData = &user_data;
    prologCallbacks.push_back(prologCallback);
    epilogCallbacks.push_back(epilogCallback);
    ze_pfnCommandListCloseCb_t apiOrdinal = {};

    result = APITracerWrapperImp(zeCommandListClose, &tracerParams, apiOrdinal, prologCallbacks, epilogCallbacks, *tracerParams.phCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(zeAPITracingCoreTests, WhenCallingTracerWrapperWithOneSetOfPrologEpilogsWithInstanceDataThenReturnSuccess) {
    MockCommandList commandList;
    ze_result_t result;
    ze_command_list_close_params_t tracerParams;
    zet_core_callbacks_t prologCbs = {};
    zet_core_callbacks_t epilogCbs = {};

    prologCbs.CommandList.pfnCloseCb = OnEnterCommandListCloseWithoutUserDataAndAllocateInstanceData;
    epilogCbs.CommandList.pfnCloseCb = OnExitCommandListCloseWithoutUserDataAndReadInstanceData;

    ze_command_list_handle_t command_list_handle = commandList.toHandle();
    tracerParams.phCommandList = &command_list_handle;

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

    result = APITracerWrapperImp(zeCommandListClose, &tracerParams, apiOrdinal, prologCallbacks, epilogCallbacks, *tracerParams.phCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(zeAPITracingCoreTests, WhenCallingTracerWrapperWithOneSetOfPrologEpilogsWithRecursionHandledThenSuccessIsReturned) {
    MockCommandList commandList;
    ze_result_t result;
    int user_data = 5;
    ze_command_list_close_params_t tracerParams;
    zet_core_callbacks_t prologCbs = {};
    zet_core_callbacks_t epilogCbs = {};

    prologCbs.CommandList.pfnCloseCb = OnEnterCommandListCloseWithUserDataRecursion;
    epilogCbs.CommandList.pfnCloseCb = OnExitCommandListCloseWithUserDataRecursion;

    ze_command_list_handle_t command_list_handle = commandList.toHandle();
    tracerParams.phCommandList = &command_list_handle;

    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> prologCallbacks;
    std::vector<APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t>> epilogCallbacks;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> prologCallback;
    APITracerCallbackStateImp<ze_pfnCommandListCloseCb_t> epilogCallback;
    prologCallback.current_api_callback = prologCbs.CommandList.pfnCloseCb;
    epilogCallback.current_api_callback = epilogCbs.CommandList.pfnCloseCb;
    prologCallback.pUserData = &user_data;
    epilogCallback.pUserData = &user_data;
    prologCallbacks.push_back(prologCallback);
    epilogCallbacks.push_back(epilogCallback);
    ze_pfnCommandListCloseCb_t apiOrdinal = {};

    result = callHandleTracerRecursion(zeCommandListClose, command_list_handle);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    result = APITracerWrapperImp(zeCommandListClose, &tracerParams, apiOrdinal, prologCallbacks, epilogCallbacks, *tracerParams.phCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = callHandleTracerRecursion(zeCommandListClose, command_list_handle);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    result = callHandleTracerRecursion(zeCommandListClose, command_list_handle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    L0::tracingInProgress = 0;
}

} // namespace ult
} // namespace L0
