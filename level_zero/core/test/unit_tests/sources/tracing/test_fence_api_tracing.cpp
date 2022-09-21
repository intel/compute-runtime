/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingFenceCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.Fence.pfnCreate =
        [](ze_command_queue_handle_t hCommandQueue, const ze_fence_desc_t *desc, ze_fence_handle_t *phFence) { return ZE_RESULT_SUCCESS; };
    ze_fence_handle_t fence = {};
    ze_fence_desc_t desc = {};

    prologCbs.Fence.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.Fence.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeFenceCreateTracing(nullptr, &desc, &fence);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingFenceDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.Fence.pfnDestroy =
        [](ze_fence_handle_t hFence) { return ZE_RESULT_SUCCESS; };

    prologCbs.Fence.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.Fence.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeFenceDestroyTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingFenceHostSynchronizeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.Fence.pfnHostSynchronize =
        [](ze_fence_handle_t hFence, uint64_t timeout) { return ZE_RESULT_SUCCESS; };
    prologCbs.Fence.pfnHostSynchronizeCb = genericPrologCallbackPtr;
    epilogCbs.Fence.pfnHostSynchronizeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeFenceHostSynchronizeTracing(nullptr, 1U);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingFenceQueryStatusTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.Fence.pfnQueryStatus =
        [](ze_fence_handle_t hFence) { return ZE_RESULT_SUCCESS; };
    prologCbs.Fence.pfnQueryStatusCb = genericPrologCallbackPtr;
    epilogCbs.Fence.pfnQueryStatusCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeFenceQueryStatusTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingFenceResetTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.Fence.pfnReset =
        [](ze_fence_handle_t hFence) { return ZE_RESULT_SUCCESS; };
    prologCbs.Fence.pfnResetCb = genericPrologCallbackPtr;
    epilogCbs.Fence.pfnResetCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeFenceResetTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

struct {
    ze_command_queue_handle_t hCommandQueue0;
    ze_fence_desc_t desc0{};
    ze_fence_handle_t hFence0;
    ze_command_queue_handle_t hCommandQueue1;
    ze_fence_desc_t desc1{};
    ze_fence_handle_t hFence1;
    ze_fence_handle_t hFenceAPI;
    void *instanceData0;
    void *instanceData3;
} fence_create_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingFenceCreateTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    fence_create_args.hCommandQueue0 = generateRandomHandle<ze_command_queue_handle_t>();
    fence_create_args.hFence0 = generateRandomHandle<ze_fence_handle_t>();

    // initialize replacement argument set
    fence_create_args.hCommandQueue1 = generateRandomHandle<ze_command_queue_handle_t>();
    fence_create_args.hFence1 = generateRandomHandle<ze_fence_handle_t>();

    // initialize user instance data
    fence_create_args.instanceData0 = generateRandomHandle<void *>();
    fence_create_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Fence.pfnCreate =
        [](ze_command_queue_handle_t hCommandQueue, const ze_fence_desc_t *desc, ze_fence_handle_t *phFence) {
            EXPECT_EQ(fence_create_args.hCommandQueue1, hCommandQueue);
            EXPECT_EQ(&fence_create_args.desc1, desc);
            EXPECT_EQ(&fence_create_args.hFence1, phFence);
            EXPECT_EQ(fence_create_args.hFence1, *phFence);
            fence_create_args.hFenceAPI = generateRandomHandle<ze_fence_handle_t>();
            *phFence = fence_create_args.hFenceAPI;
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Fence.pfnCreateCb =
        [](ze_fence_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_create_args.hCommandQueue0, *params->phCommandQueue);
            EXPECT_EQ(&fence_create_args.desc0, *params->pdesc);
            EXPECT_EQ(&fence_create_args.hFence0, *params->pphFence);

            ze_fence_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphFence;

            ze_fence_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_fence_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(fence_create_args.hFence0, handle);
            *params->phCommandQueue = fence_create_args.hCommandQueue1;
            *params->pdesc = &fence_create_args.desc1;
            *params->pphFence = &fence_create_args.hFence1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = fence_create_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Fence.pfnCreateCb =
        [](ze_fence_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_create_args.hCommandQueue1, *params->phCommandQueue);
            EXPECT_EQ(&fence_create_args.desc1, *params->pdesc);
            EXPECT_EQ(&fence_create_args.hFence1, *params->pphFence);

            ze_fence_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphFence;

            ze_fence_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_fence_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(fence_create_args.hFence1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, fence_create_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Fence.pfnCreateCb =
        [](ze_fence_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_create_args.hCommandQueue1, *params->phCommandQueue);
            EXPECT_EQ(&fence_create_args.desc1, *params->pdesc);
            EXPECT_EQ(&fence_create_args.hFence1, *params->pphFence);

            ze_fence_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphFence;

            ze_fence_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_fence_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(fence_create_args.hFence1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Fence.pfnCreateCb =
        [](ze_fence_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_create_args.hCommandQueue1, *params->phCommandQueue);
            EXPECT_EQ(&fence_create_args.desc1, *params->pdesc);
            EXPECT_EQ(&fence_create_args.hFence1, *params->pphFence);

            ze_fence_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphFence;

            ze_fence_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_fence_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(fence_create_args.hFence1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Fence.pfnCreateCb =
        [](ze_fence_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_create_args.hCommandQueue1, *params->phCommandQueue);
            EXPECT_EQ(&fence_create_args.desc1, *params->pdesc);
            EXPECT_EQ(&fence_create_args.hFence1, *params->pphFence);

            ze_fence_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphFence;

            ze_fence_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_fence_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(fence_create_args.hFence1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = fence_create_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Fence.pfnCreateCb =
        [](ze_fence_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_create_args.hCommandQueue1, *params->phCommandQueue);
            EXPECT_EQ(&fence_create_args.desc1, *params->pdesc);
            EXPECT_EQ(&fence_create_args.hFence1, *params->pphFence);

            ze_fence_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphFence;

            ze_fence_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_fence_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            ASSERT_NE(nullptr, handle);
            EXPECT_EQ(fence_create_args.hFence1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, fence_create_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeFenceCreateTracing(fence_create_args.hCommandQueue0, &fence_create_args.desc0, &fence_create_args.hFence0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(fence_create_args.hFence1, fence_create_args.hFenceAPI);
    validateDefaultUserDataFinal();
}

struct {
    ze_fence_handle_t hFence0;
    ze_fence_handle_t hFence1;
    void *instanceData0;
    void *instanceData3;
} fence_destroy_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingFenceDestroyTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    fence_destroy_args.hFence0 = generateRandomHandle<ze_fence_handle_t>();

    // initialize replacement argument set
    fence_destroy_args.hFence1 = generateRandomHandle<ze_fence_handle_t>();

    // initialize user instance data
    fence_destroy_args.instanceData0 = generateRandomHandle<void *>();
    fence_destroy_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Fence.pfnDestroy =
        [](ze_fence_handle_t hFence) {
            EXPECT_EQ(fence_destroy_args.hFence1, hFence);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Fence.pfnDestroyCb =
        [](ze_fence_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_destroy_args.hFence0, *params->phFence);
            *params->phFence = fence_destroy_args.hFence1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = fence_destroy_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Fence.pfnDestroyCb =
        [](ze_fence_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_destroy_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, fence_destroy_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Fence.pfnDestroyCb =
        [](ze_fence_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_destroy_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Fence.pfnDestroyCb =
        [](ze_fence_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_destroy_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Fence.pfnDestroyCb =
        [](ze_fence_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_destroy_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = fence_destroy_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Fence.pfnDestroyCb =
        [](ze_fence_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_destroy_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, fence_destroy_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeFenceDestroyTracing(fence_destroy_args.hFence0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_fence_handle_t hFence0;
    uint64_t timeout0;
    ze_fence_handle_t hFence1;
    uint64_t timeout1;
    void *instanceData0;
    void *instanceData3;
} fence_host_synchronize_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingFenceHostSynchronizeTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    fence_host_synchronize_args.hFence0 = generateRandomHandle<ze_fence_handle_t>();
    fence_host_synchronize_args.timeout0 = generateRandomSize<uint64_t>();

    // initialize replacement argument set
    fence_host_synchronize_args.hFence1 = generateRandomHandle<ze_fence_handle_t>();
    fence_host_synchronize_args.timeout1 = generateRandomSize<uint64_t>();

    // initialize user instance data
    fence_host_synchronize_args.instanceData0 = generateRandomHandle<void *>();
    fence_host_synchronize_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Fence.pfnHostSynchronize =
        [](ze_fence_handle_t hFence, uint64_t timeout) {
            EXPECT_EQ(fence_host_synchronize_args.hFence1, hFence);
            EXPECT_EQ(fence_host_synchronize_args.timeout1, timeout);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Fence.pfnHostSynchronizeCb =
        [](ze_fence_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_host_synchronize_args.hFence0, *params->phFence);
            EXPECT_EQ(fence_host_synchronize_args.timeout0, *params->ptimeout);
            *params->phFence = fence_host_synchronize_args.hFence1;
            *params->ptimeout = fence_host_synchronize_args.timeout1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = fence_host_synchronize_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Fence.pfnHostSynchronizeCb =
        [](ze_fence_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_host_synchronize_args.hFence1, *params->phFence);
            EXPECT_EQ(fence_host_synchronize_args.timeout1, *params->ptimeout);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, fence_host_synchronize_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Fence.pfnHostSynchronizeCb =
        [](ze_fence_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_host_synchronize_args.hFence1, *params->phFence);
            EXPECT_EQ(fence_host_synchronize_args.timeout1, *params->ptimeout);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Fence.pfnHostSynchronizeCb =
        [](ze_fence_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_host_synchronize_args.hFence1, *params->phFence);
            EXPECT_EQ(fence_host_synchronize_args.timeout1, *params->ptimeout);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Fence.pfnHostSynchronizeCb =
        [](ze_fence_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_host_synchronize_args.hFence1, *params->phFence);
            EXPECT_EQ(fence_host_synchronize_args.timeout1, *params->ptimeout);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = fence_host_synchronize_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Fence.pfnHostSynchronizeCb =
        [](ze_fence_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_host_synchronize_args.hFence1, *params->phFence);
            EXPECT_EQ(fence_host_synchronize_args.timeout1, *params->ptimeout);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, fence_host_synchronize_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeFenceHostSynchronizeTracing(fence_host_synchronize_args.hFence0, fence_host_synchronize_args.timeout0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_fence_handle_t hFence0;
    ze_fence_handle_t hFence1;
    void *instanceData0;
    void *instanceData3;
} fence_query_status_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingFenceQueryStatusTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    fence_query_status_args.hFence0 = generateRandomHandle<ze_fence_handle_t>();

    // initialize replacement argument set
    fence_query_status_args.hFence1 = generateRandomHandle<ze_fence_handle_t>();

    // initialize user instance data
    fence_query_status_args.instanceData0 = generateRandomHandle<void *>();
    fence_query_status_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Fence.pfnQueryStatus =
        [](ze_fence_handle_t hFence) {
            EXPECT_EQ(fence_query_status_args.hFence1, hFence);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Fence.pfnQueryStatusCb =
        [](ze_fence_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_query_status_args.hFence0, *params->phFence);
            *params->phFence = fence_query_status_args.hFence1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = fence_query_status_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Fence.pfnQueryStatusCb =
        [](ze_fence_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_query_status_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, fence_query_status_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Fence.pfnQueryStatusCb =
        [](ze_fence_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_query_status_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Fence.pfnQueryStatusCb =
        [](ze_fence_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_query_status_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Fence.pfnQueryStatusCb =
        [](ze_fence_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_query_status_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = fence_query_status_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Fence.pfnQueryStatusCb =
        [](ze_fence_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_query_status_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, fence_query_status_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeFenceQueryStatusTracing(fence_query_status_args.hFence0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_fence_handle_t hFence0;
    ze_fence_handle_t hFence1;
    void *instanceData0;
    void *instanceData3;
} fence_reset_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingFenceResetTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    fence_reset_args.hFence0 = generateRandomHandle<ze_fence_handle_t>();

    // initialize replacement argument set
    fence_reset_args.hFence1 = generateRandomHandle<ze_fence_handle_t>();

    // initialize user instance data
    fence_reset_args.instanceData0 = generateRandomHandle<void *>();
    fence_reset_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Fence.pfnReset =
        [](ze_fence_handle_t hFence) {
            EXPECT_EQ(fence_reset_args.hFence1, hFence);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Fence.pfnResetCb =
        [](ze_fence_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_reset_args.hFence0, *params->phFence);
            *params->phFence = fence_reset_args.hFence1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = fence_reset_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Fence.pfnResetCb =
        [](ze_fence_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_reset_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, fence_reset_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Fence.pfnResetCb =
        [](ze_fence_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_reset_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Fence.pfnResetCb =
        [](ze_fence_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_reset_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Fence.pfnResetCb =
        [](ze_fence_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(fence_reset_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = fence_reset_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Fence.pfnResetCb =
        [](ze_fence_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(fence_reset_args.hFence1, *params->phFence);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, fence_reset_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeFenceResetTracing(fence_reset_args.hFence0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

} // namespace ult
} // namespace L0
