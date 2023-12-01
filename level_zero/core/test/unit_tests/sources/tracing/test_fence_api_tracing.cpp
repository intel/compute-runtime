/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingFenceCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Fence.pfnCreate =
        [](ze_command_queue_handle_t hCommandQueue, const ze_fence_desc_t *desc, ze_fence_handle_t *phFence) -> ze_result_t { return ZE_RESULT_SUCCESS; };
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
    driverDdiTable.coreDdiTable.Fence.pfnDestroy =
        [](ze_fence_handle_t hFence) -> ze_result_t { return ZE_RESULT_SUCCESS; };

    prologCbs.Fence.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.Fence.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeFenceDestroyTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingFenceHostSynchronizeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Fence.pfnHostSynchronize =
        [](ze_fence_handle_t hFence, uint64_t timeout) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    prologCbs.Fence.pfnHostSynchronizeCb = genericPrologCallbackPtr;
    epilogCbs.Fence.pfnHostSynchronizeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeFenceHostSynchronizeTracing(nullptr, 1U);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingFenceQueryStatusTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Fence.pfnQueryStatus =
        [](ze_fence_handle_t hFence) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    prologCbs.Fence.pfnQueryStatusCb = genericPrologCallbackPtr;
    epilogCbs.Fence.pfnQueryStatusCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeFenceQueryStatusTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingFenceResetTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Fence.pfnReset =
        [](ze_fence_handle_t hFence) -> ze_result_t { return ZE_RESULT_SUCCESS; };
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
} fenceCreateArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingFenceCreateTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    fenceCreateArgs.hCommandQueue0 = generateRandomHandle<ze_command_queue_handle_t>();
    fenceCreateArgs.hFence0 = generateRandomHandle<ze_fence_handle_t>();

    // initialize replacement argument set
    fenceCreateArgs.hCommandQueue1 = generateRandomHandle<ze_command_queue_handle_t>();
    fenceCreateArgs.hFence1 = generateRandomHandle<ze_fence_handle_t>();

    // initialize user instance data
    fenceCreateArgs.instanceData0 = generateRandomHandle<void *>();
    fenceCreateArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Fence.pfnCreate =
        [](ze_command_queue_handle_t hCommandQueue, const ze_fence_desc_t *desc, ze_fence_handle_t *phFence) -> ze_result_t {
        EXPECT_EQ(fenceCreateArgs.hCommandQueue1, hCommandQueue);
        EXPECT_EQ(&fenceCreateArgs.desc1, desc);
        EXPECT_EQ(&fenceCreateArgs.hFence1, phFence);
        EXPECT_EQ(fenceCreateArgs.hFence1, *phFence);
        fenceCreateArgs.hFenceAPI = generateRandomHandle<ze_fence_handle_t>();
        *phFence = fenceCreateArgs.hFenceAPI;
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Fence.pfnCreateCb =
        [](ze_fence_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceCreateArgs.hCommandQueue0, *params->phCommandQueue);
        EXPECT_EQ(&fenceCreateArgs.desc0, *params->pdesc);
        EXPECT_EQ(&fenceCreateArgs.hFence0, *params->pphFence);

        ze_fence_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphFence;

        ze_fence_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_fence_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(fenceCreateArgs.hFence0, handle);
        *params->phCommandQueue = fenceCreateArgs.hCommandQueue1;
        *params->pdesc = &fenceCreateArgs.desc1;
        *params->pphFence = &fenceCreateArgs.hFence1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = fenceCreateArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Fence.pfnCreateCb =
        [](ze_fence_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceCreateArgs.hCommandQueue1, *params->phCommandQueue);
        EXPECT_EQ(&fenceCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&fenceCreateArgs.hFence1, *params->pphFence);

        ze_fence_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphFence;

        ze_fence_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_fence_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(fenceCreateArgs.hFence1, handle);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, fenceCreateArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Fence.pfnCreateCb =
        [](ze_fence_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceCreateArgs.hCommandQueue1, *params->phCommandQueue);
        EXPECT_EQ(&fenceCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&fenceCreateArgs.hFence1, *params->pphFence);

        ze_fence_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphFence;

        ze_fence_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_fence_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(fenceCreateArgs.hFence1, handle);
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
        [](ze_fence_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceCreateArgs.hCommandQueue1, *params->phCommandQueue);
        EXPECT_EQ(&fenceCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&fenceCreateArgs.hFence1, *params->pphFence);

        ze_fence_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphFence;

        ze_fence_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_fence_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(fenceCreateArgs.hFence1, handle);
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
        [](ze_fence_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceCreateArgs.hCommandQueue1, *params->phCommandQueue);
        EXPECT_EQ(&fenceCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&fenceCreateArgs.hFence1, *params->pphFence);

        ze_fence_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphFence;

        ze_fence_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_fence_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(fenceCreateArgs.hFence1, handle);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = fenceCreateArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Fence.pfnCreateCb =
        [](ze_fence_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceCreateArgs.hCommandQueue1, *params->phCommandQueue);
        EXPECT_EQ(&fenceCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&fenceCreateArgs.hFence1, *params->pphFence);

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
        EXPECT_EQ(fenceCreateArgs.hFence1, handle);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, fenceCreateArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeFenceCreateTracing(fenceCreateArgs.hCommandQueue0, &fenceCreateArgs.desc0, &fenceCreateArgs.hFence0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(fenceCreateArgs.hFence1, fenceCreateArgs.hFenceAPI);
    validateDefaultUserDataFinal();
}

struct {
    ze_fence_handle_t hFence0;
    ze_fence_handle_t hFence1;
    void *instanceData0;
    void *instanceData3;
} fenceDestroyArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingFenceDestroyTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    fenceDestroyArgs.hFence0 = generateRandomHandle<ze_fence_handle_t>();

    // initialize replacement argument set
    fenceDestroyArgs.hFence1 = generateRandomHandle<ze_fence_handle_t>();

    // initialize user instance data
    fenceDestroyArgs.instanceData0 = generateRandomHandle<void *>();
    fenceDestroyArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Fence.pfnDestroy =
        [](ze_fence_handle_t hFence) -> ze_result_t {
        EXPECT_EQ(fenceDestroyArgs.hFence1, hFence);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Fence.pfnDestroyCb =
        [](ze_fence_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceDestroyArgs.hFence0, *params->phFence);
        *params->phFence = fenceDestroyArgs.hFence1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = fenceDestroyArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Fence.pfnDestroyCb =
        [](ze_fence_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceDestroyArgs.hFence1, *params->phFence);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, fenceDestroyArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Fence.pfnDestroyCb =
        [](ze_fence_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceDestroyArgs.hFence1, *params->phFence);
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
        [](ze_fence_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceDestroyArgs.hFence1, *params->phFence);
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
        [](ze_fence_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceDestroyArgs.hFence1, *params->phFence);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = fenceDestroyArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Fence.pfnDestroyCb =
        [](ze_fence_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceDestroyArgs.hFence1, *params->phFence);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, fenceDestroyArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeFenceDestroyTracing(fenceDestroyArgs.hFence0);
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
} fenceHostSynchronizeArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingFenceHostSynchronizeTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    fenceHostSynchronizeArgs.hFence0 = generateRandomHandle<ze_fence_handle_t>();
    fenceHostSynchronizeArgs.timeout0 = generateRandomSize<uint64_t>();

    // initialize replacement argument set
    fenceHostSynchronizeArgs.hFence1 = generateRandomHandle<ze_fence_handle_t>();
    fenceHostSynchronizeArgs.timeout1 = generateRandomSize<uint64_t>();

    // initialize user instance data
    fenceHostSynchronizeArgs.instanceData0 = generateRandomHandle<void *>();
    fenceHostSynchronizeArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Fence.pfnHostSynchronize =
        [](ze_fence_handle_t hFence, uint64_t timeout) -> ze_result_t {
        EXPECT_EQ(fenceHostSynchronizeArgs.hFence1, hFence);
        EXPECT_EQ(fenceHostSynchronizeArgs.timeout1, timeout);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Fence.pfnHostSynchronizeCb =
        [](ze_fence_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceHostSynchronizeArgs.hFence0, *params->phFence);
        EXPECT_EQ(fenceHostSynchronizeArgs.timeout0, *params->ptimeout);
        *params->phFence = fenceHostSynchronizeArgs.hFence1;
        *params->ptimeout = fenceHostSynchronizeArgs.timeout1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = fenceHostSynchronizeArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Fence.pfnHostSynchronizeCb =
        [](ze_fence_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceHostSynchronizeArgs.hFence1, *params->phFence);
        EXPECT_EQ(fenceHostSynchronizeArgs.timeout1, *params->ptimeout);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, fenceHostSynchronizeArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Fence.pfnHostSynchronizeCb =
        [](ze_fence_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceHostSynchronizeArgs.hFence1, *params->phFence);
        EXPECT_EQ(fenceHostSynchronizeArgs.timeout1, *params->ptimeout);
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
        [](ze_fence_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceHostSynchronizeArgs.hFence1, *params->phFence);
        EXPECT_EQ(fenceHostSynchronizeArgs.timeout1, *params->ptimeout);
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
        [](ze_fence_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceHostSynchronizeArgs.hFence1, *params->phFence);
        EXPECT_EQ(fenceHostSynchronizeArgs.timeout1, *params->ptimeout);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = fenceHostSynchronizeArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Fence.pfnHostSynchronizeCb =
        [](ze_fence_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceHostSynchronizeArgs.hFence1, *params->phFence);
        EXPECT_EQ(fenceHostSynchronizeArgs.timeout1, *params->ptimeout);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, fenceHostSynchronizeArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeFenceHostSynchronizeTracing(fenceHostSynchronizeArgs.hFence0, fenceHostSynchronizeArgs.timeout0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_fence_handle_t hFence0;
    ze_fence_handle_t hFence1;
    void *instanceData0;
    void *instanceData3;
} fenceQueryStatusArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingFenceQueryStatusTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    fenceQueryStatusArgs.hFence0 = generateRandomHandle<ze_fence_handle_t>();

    // initialize replacement argument set
    fenceQueryStatusArgs.hFence1 = generateRandomHandle<ze_fence_handle_t>();

    // initialize user instance data
    fenceQueryStatusArgs.instanceData0 = generateRandomHandle<void *>();
    fenceQueryStatusArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Fence.pfnQueryStatus =
        [](ze_fence_handle_t hFence) -> ze_result_t {
        EXPECT_EQ(fenceQueryStatusArgs.hFence1, hFence);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Fence.pfnQueryStatusCb =
        [](ze_fence_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceQueryStatusArgs.hFence0, *params->phFence);
        *params->phFence = fenceQueryStatusArgs.hFence1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = fenceQueryStatusArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Fence.pfnQueryStatusCb =
        [](ze_fence_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceQueryStatusArgs.hFence1, *params->phFence);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, fenceQueryStatusArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Fence.pfnQueryStatusCb =
        [](ze_fence_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceQueryStatusArgs.hFence1, *params->phFence);
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
        [](ze_fence_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceQueryStatusArgs.hFence1, *params->phFence);
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
        [](ze_fence_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceQueryStatusArgs.hFence1, *params->phFence);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = fenceQueryStatusArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Fence.pfnQueryStatusCb =
        [](ze_fence_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceQueryStatusArgs.hFence1, *params->phFence);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, fenceQueryStatusArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeFenceQueryStatusTracing(fenceQueryStatusArgs.hFence0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_fence_handle_t hFence0;
    ze_fence_handle_t hFence1;
    void *instanceData0;
    void *instanceData3;
} fenceResetArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingFenceResetTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    fenceResetArgs.hFence0 = generateRandomHandle<ze_fence_handle_t>();

    // initialize replacement argument set
    fenceResetArgs.hFence1 = generateRandomHandle<ze_fence_handle_t>();

    // initialize user instance data
    fenceResetArgs.instanceData0 = generateRandomHandle<void *>();
    fenceResetArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Fence.pfnReset =
        [](ze_fence_handle_t hFence) -> ze_result_t {
        EXPECT_EQ(fenceResetArgs.hFence1, hFence);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Fence.pfnResetCb =
        [](ze_fence_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceResetArgs.hFence0, *params->phFence);
        *params->phFence = fenceResetArgs.hFence1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = fenceResetArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Fence.pfnResetCb =
        [](ze_fence_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceResetArgs.hFence1, *params->phFence);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, fenceResetArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Fence.pfnResetCb =
        [](ze_fence_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceResetArgs.hFence1, *params->phFence);
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
        [](ze_fence_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceResetArgs.hFence1, *params->phFence);
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
        [](ze_fence_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(fenceResetArgs.hFence1, *params->phFence);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = fenceResetArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Fence.pfnResetCb =
        [](ze_fence_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(fenceResetArgs.hFence1, *params->phFence);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, fenceResetArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeFenceResetTracing(fenceResetArgs.hFence0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

} // namespace ult
} // namespace L0
