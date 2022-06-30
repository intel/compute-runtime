/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

// Multi prolog/epilog Event Tests

struct {
    ze_event_pool_handle_t hEventPool0;
    ze_event_desc_t desc0;
    ze_event_handle_t hEvent0;
    ze_event_pool_handle_t hEventPool1;
    ze_event_desc_t desc1;
    ze_event_handle_t hEvent1;
    ze_event_handle_t hEventAPI;
    void *instanceData0;
    void *instanceData3;
} event_create_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventCreateTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    event_create_args.hEventPool0 = generateRandomHandle<ze_event_pool_handle_t>();
    event_create_args.hEvent0 = generateRandomHandle<ze_event_handle_t>();

    // initialize replacement argument set
    event_create_args.hEventPool1 = generateRandomHandle<ze_event_pool_handle_t>();
    event_create_args.hEvent1 = generateRandomHandle<ze_event_handle_t>();

    // initialize user instance data
    event_create_args.instanceData0 = generateRandomHandle<void *>();
    event_create_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Event.pfnCreate =
        [](ze_event_pool_handle_t hEventPool, const ze_event_desc_t *desc, ze_event_handle_t *phEvent) {
            EXPECT_EQ(event_create_args.hEventPool1, hEventPool);
            EXPECT_EQ(&event_create_args.desc1, desc);
            EXPECT_EQ(&event_create_args.hEvent1, phEvent);
            EXPECT_EQ(event_create_args.hEvent1, *phEvent);
            event_create_args.hEventAPI = generateRandomHandle<ze_event_handle_t>();
            *phEvent = event_create_args.hEventAPI;
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Event.pfnCreateCb =
        [](ze_event_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_create_args.hEventPool0, *params->phEventPool);
            EXPECT_EQ(&event_create_args.desc0, *params->pdesc);
            EXPECT_EQ(&event_create_args.hEvent0, *params->pphEvent);

            ze_event_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEvent;

            ze_event_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_event_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(event_create_args.hEvent0, handle);
            *params->phEventPool = event_create_args.hEventPool1;
            *params->pdesc = &event_create_args.desc1;
            *params->pphEvent = &event_create_args.hEvent1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_create_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Event.pfnCreateCb =
        [](ze_event_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_create_args.hEventPool1, *params->phEventPool);
            EXPECT_EQ(&event_create_args.desc1, *params->pdesc);
            EXPECT_EQ(&event_create_args.hEvent1, *params->pphEvent);

            ze_event_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEvent;

            ze_event_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_event_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(event_create_args.hEvent1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_create_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Event.pfnCreateCb =
        [](ze_event_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_create_args.hEventPool1, *params->phEventPool);
            EXPECT_EQ(&event_create_args.desc1, *params->pdesc);
            EXPECT_EQ(&event_create_args.hEvent1, *params->pphEvent);

            ze_event_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEvent;

            ze_event_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_event_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(event_create_args.hEvent1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Event.pfnCreateCb =
        [](ze_event_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_create_args.hEventPool1, *params->phEventPool);
            EXPECT_EQ(&event_create_args.desc1, *params->pdesc);
            EXPECT_EQ(&event_create_args.hEvent1, *params->pphEvent);

            ze_event_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEvent;

            ze_event_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_event_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(event_create_args.hEvent1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Event.pfnCreateCb =
        [](ze_event_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_create_args.hEventPool1, *params->phEventPool);
            EXPECT_EQ(&event_create_args.desc1, *params->pdesc);
            EXPECT_EQ(&event_create_args.hEvent1, *params->pphEvent);

            ze_event_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEvent;

            ze_event_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_event_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(event_create_args.hEvent1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_create_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Event.pfnCreateCb =
        [](ze_event_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_create_args.hEventPool1, *params->phEventPool);
            EXPECT_EQ(&event_create_args.desc1, *params->pdesc);
            EXPECT_EQ(&event_create_args.hEvent1, *params->pphEvent);

            ze_event_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEvent;

            ze_event_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_event_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(event_create_args.hEvent1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_create_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeEventCreateTracing(event_create_args.hEventPool0, &event_create_args.desc0, &event_create_args.hEvent0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_handle_t hEvent0;
    ze_event_handle_t hEvent1;
    void *instanceData0;
    void *instanceData3;
} event_destroy_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventDestroyTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    event_destroy_args.hEvent0 = generateRandomHandle<ze_event_handle_t>();

    // initialize replacement argument set
    event_destroy_args.hEvent1 = generateRandomHandle<ze_event_handle_t>();

    // initialize user instance data
    event_destroy_args.instanceData0 = generateRandomHandle<void *>();
    event_destroy_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Event.pfnDestroy =
        [](ze_event_handle_t hEvent) {
            EXPECT_EQ(event_destroy_args.hEvent1, hEvent);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Event.pfnDestroyCb =
        [](ze_event_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_destroy_args.hEvent0, *params->phEvent);
            *params->phEvent = event_destroy_args.hEvent1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_destroy_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Event.pfnDestroyCb =
        [](ze_event_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_destroy_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_destroy_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Event.pfnDestroyCb =
        [](ze_event_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_destroy_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Event.pfnDestroyCb =
        [](ze_event_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_destroy_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Event.pfnDestroyCb =
        [](ze_event_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_destroy_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_destroy_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Event.pfnDestroyCb =
        [](ze_event_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_destroy_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_destroy_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeEventDestroyTracing(event_destroy_args.hEvent0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_handle_t hEvent0;
    ze_event_handle_t hEvent1;
    void *instanceData0;
    void *instanceData3;
} event_host_signal_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventHostSignalTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    event_host_signal_args.hEvent0 = generateRandomHandle<ze_event_handle_t>();

    // initialize replacement argument set
    event_host_signal_args.hEvent1 = generateRandomHandle<ze_event_handle_t>();

    // initialize user instance data
    event_host_signal_args.instanceData0 = generateRandomHandle<void *>();
    event_host_signal_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Event.pfnHostSignal =
        [](ze_event_handle_t hEvent) {
            EXPECT_EQ(event_host_signal_args.hEvent1, hEvent);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Event.pfnHostSignalCb =
        [](ze_event_host_signal_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_host_signal_args.hEvent0, *params->phEvent);
            *params->phEvent = event_host_signal_args.hEvent1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_host_signal_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Event.pfnHostSignalCb =
        [](ze_event_host_signal_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_host_signal_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_host_signal_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Event.pfnHostSignalCb =
        [](ze_event_host_signal_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_host_signal_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Event.pfnHostSignalCb =
        [](ze_event_host_signal_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_host_signal_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Event.pfnHostSignalCb =
        [](ze_event_host_signal_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_host_signal_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_host_signal_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Event.pfnHostSignalCb =
        [](ze_event_host_signal_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_host_signal_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_host_signal_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeEventHostSignalTracing(event_host_signal_args.hEvent0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_handle_t hEvent0;
    uint32_t timeout0;
    ze_event_handle_t hEvent1;
    uint32_t timeout1;
    void *instanceData0;
    void *instanceData3;
} event_host_synchronize_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventHostSynchronizeTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    event_host_synchronize_args.hEvent0 = generateRandomHandle<ze_event_handle_t>();
    event_host_synchronize_args.timeout0 = generateRandomSize<uint32_t>();

    // initialize replacement argument set
    event_host_synchronize_args.hEvent1 = generateRandomHandle<ze_event_handle_t>();
    event_host_synchronize_args.timeout1 = generateRandomSize<uint32_t>();

    // initialize user instance data
    event_host_synchronize_args.instanceData0 = generateRandomHandle<void *>();
    event_host_synchronize_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Event.pfnHostSynchronize =
        [](ze_event_handle_t hEvent, uint64_t timeout) {
            EXPECT_EQ(event_host_synchronize_args.hEvent1, hEvent);
            EXPECT_EQ(event_host_synchronize_args.timeout1, timeout);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Event.pfnHostSynchronizeCb =
        [](ze_event_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_host_synchronize_args.hEvent0, *params->phEvent);
            EXPECT_EQ(event_host_synchronize_args.timeout0, *params->ptimeout);
            *params->phEvent = event_host_synchronize_args.hEvent1;
            *params->ptimeout = event_host_synchronize_args.timeout1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_host_synchronize_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Event.pfnHostSynchronizeCb =
        [](ze_event_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_host_synchronize_args.hEvent1, *params->phEvent);
            EXPECT_EQ(event_host_synchronize_args.timeout1, *params->ptimeout);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_host_synchronize_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Event.pfnHostSynchronizeCb =
        [](ze_event_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_host_synchronize_args.hEvent1, *params->phEvent);
            EXPECT_EQ(event_host_synchronize_args.timeout1, *params->ptimeout);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Event.pfnHostSynchronizeCb =
        [](ze_event_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            ASSERT_NE(nullptr, pTracerUserData);
            EXPECT_EQ(event_host_synchronize_args.hEvent1, *params->phEvent);
            EXPECT_EQ(event_host_synchronize_args.timeout1, *params->ptimeout);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Event.pfnHostSynchronizeCb =
        [](ze_event_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_host_synchronize_args.hEvent1, *params->phEvent);
            EXPECT_EQ(event_host_synchronize_args.timeout1, *params->ptimeout);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_host_synchronize_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Event.pfnHostSynchronizeCb =
        [](ze_event_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_host_synchronize_args.hEvent1, *params->phEvent);
            EXPECT_EQ(event_host_synchronize_args.timeout1, *params->ptimeout);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_host_synchronize_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeEventHostSynchronizeTracing(event_host_synchronize_args.hEvent0, event_host_synchronize_args.timeout0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_handle_t hEvent0;
    ze_event_handle_t hEvent1;
    void *instanceData0;
    void *instanceData3;
} event_query_status_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventQueryStatusTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    event_query_status_args.hEvent0 = generateRandomHandle<ze_event_handle_t>();

    // initialize replacement argument set
    event_query_status_args.hEvent1 = generateRandomHandle<ze_event_handle_t>();

    // initialize user instance data
    event_query_status_args.instanceData0 = generateRandomHandle<void *>();
    event_query_status_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Event.pfnQueryStatus =
        [](ze_event_handle_t hEvent) {
            EXPECT_EQ(event_query_status_args.hEvent1, hEvent);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Event.pfnQueryStatusCb =
        [](ze_event_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_query_status_args.hEvent0, *params->phEvent);
            *params->phEvent = event_query_status_args.hEvent1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_query_status_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Event.pfnQueryStatusCb =
        [](ze_event_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_query_status_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_query_status_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Event.pfnQueryStatusCb =
        [](ze_event_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_query_status_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Event.pfnQueryStatusCb =
        [](ze_event_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            ASSERT_NE(nullptr, pTracerUserData);
            EXPECT_EQ(event_query_status_args.hEvent1, *params->phEvent);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Event.pfnQueryStatusCb =
        [](ze_event_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_query_status_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_query_status_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Event.pfnQueryStatusCb =
        [](ze_event_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_query_status_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_query_status_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeEventQueryStatusTracing(event_query_status_args.hEvent0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_handle_t hEvent0;
    ze_event_handle_t hEvent1;
    void *instanceData0;
    void *instanceData3;
} event_reset_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventHostResetTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    event_reset_args.hEvent0 = generateRandomHandle<ze_event_handle_t>();

    // initialize replacement argument set
    event_reset_args.hEvent1 = generateRandomHandle<ze_event_handle_t>();

    // initialize user instance data
    event_reset_args.instanceData0 = generateRandomHandle<void *>();
    event_reset_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Event.pfnHostReset =
        [](ze_event_handle_t hEvent) {
            EXPECT_EQ(event_reset_args.hEvent1, hEvent);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Event.pfnHostResetCb =
        [](ze_event_host_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_reset_args.hEvent0, *params->phEvent);
            *params->phEvent = event_reset_args.hEvent1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_reset_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Event.pfnHostResetCb =
        [](ze_event_host_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_reset_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_reset_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Event.pfnHostResetCb =
        [](ze_event_host_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_reset_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Event.pfnHostResetCb =
        [](ze_event_host_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            ASSERT_NE(nullptr, pTracerUserData);
            EXPECT_EQ(event_reset_args.hEvent1, *params->phEvent);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Event.pfnHostResetCb =
        [](ze_event_host_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_reset_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_reset_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Event.pfnHostResetCb =
        [](ze_event_host_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_reset_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_reset_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeEventHostResetTracing(event_reset_args.hEvent0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

// Multi prolog/epilog Event Pool Tests

#define NUM_EVENT_POOL_CREATE_DEVICES_0 2
#define NUM_EVENT_POOL_CREATE_DEVICES_1 4

struct {
    ze_context_handle_t hContext0;
    ze_event_pool_desc_t desc0;
    uint32_t numDevices0 = NUM_EVENT_POOL_CREATE_DEVICES_0;
    ze_device_handle_t hDevices0[NUM_EVENT_POOL_CREATE_DEVICES_0];
    ze_event_pool_handle_t hEventPool0;
    ze_context_handle_t hContext1;
    ze_event_pool_desc_t desc1;
    uint32_t numDevices1 = NUM_EVENT_POOL_CREATE_DEVICES_1;
    ze_device_handle_t hDevices1[NUM_EVENT_POOL_CREATE_DEVICES_1];
    ze_event_pool_handle_t hEventPool1;
    ze_event_pool_handle_t hEventPoolAPI;
    void *instanceData0;
    void *instanceData3;
} event_pool_create_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventPoolCreateTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    event_pool_create_args.hContext0 = generateRandomHandle<ze_context_handle_t>();
    for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
        event_pool_create_args.hDevices0[i] = generateRandomHandle<ze_device_handle_t>();
    }
    event_pool_create_args.hEventPool0 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize replacement argument set
    event_pool_create_args.hContext1 = generateRandomHandle<ze_context_handle_t>();
    for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_1; i++) {
        event_pool_create_args.hDevices1[i] = generateRandomHandle<ze_device_handle_t>();
    }
    event_pool_create_args.hEventPool1 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize user instance data
    event_pool_create_args.instanceData0 = generateRandomHandle<void *>();
    event_pool_create_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.EventPool.pfnCreate =
        [](ze_context_handle_t hContext, const ze_event_pool_desc_t *desc, uint32_t numDevices, ze_device_handle_t *phDevices, ze_event_pool_handle_t *phEventPool) {
            EXPECT_EQ(event_pool_create_args.hContext1, hContext);
            EXPECT_EQ(&event_pool_create_args.desc1, desc);
            EXPECT_EQ(event_pool_create_args.numDevices1, numDevices);
            EXPECT_EQ(event_pool_create_args.hDevices1, phDevices);
            for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_1; i++) {
                EXPECT_EQ(event_pool_create_args.hDevices1[i], phDevices[i]);
            }
            EXPECT_EQ(event_pool_create_args.hEventPool1, *phEventPool);
            EXPECT_EQ(&event_pool_create_args.hEventPool1, phEventPool);
            event_pool_create_args.hEventPoolAPI = generateRandomHandle<ze_event_pool_handle_t>();
            *phEventPool = event_pool_create_args.hEventPoolAPI;
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.EventPool.pfnCreateCb =
        [](ze_event_pool_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_create_args.hContext0, *params->phContext);
            EXPECT_EQ(&event_pool_create_args.desc0, *params->pdesc);
            EXPECT_EQ(event_pool_create_args.numDevices0, *params->pnumDevices);
            EXPECT_EQ(event_pool_create_args.hDevices0, *params->pphDevices);
            for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
                EXPECT_EQ(event_pool_create_args.hDevices0[i], (*(params->pphDevices))[i]);
            }

            ze_event_pool_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEventPool;

            ze_event_pool_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            EXPECT_EQ(&event_pool_create_args.hEventPool0, pHandle);

            ze_event_pool_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(event_pool_create_args.hEventPool0, handle);

            *params->phContext = event_pool_create_args.hContext1;
            *params->pdesc = &event_pool_create_args.desc1;
            *params->pnumDevices = event_pool_create_args.numDevices1;
            *params->pphDevices = event_pool_create_args.hDevices1;
            *params->pphEventPool = &event_pool_create_args.hEventPool1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_create_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.EventPool.pfnCreateCb =
        [](ze_event_pool_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_create_args.hContext1, *params->phContext);
            EXPECT_EQ(&event_pool_create_args.desc1, *params->pdesc);
            EXPECT_EQ(event_pool_create_args.numDevices1, *params->pnumDevices);
            EXPECT_EQ(event_pool_create_args.hDevices1, *params->pphDevices);
            for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
                EXPECT_EQ(event_pool_create_args.hDevices1[i], (*(params->pphDevices))[i]);
            }

            ze_event_pool_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEventPool;

            ze_event_pool_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            EXPECT_EQ(&event_pool_create_args.hEventPool1, pHandle);

            ze_event_pool_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(event_pool_create_args.hEventPool1, handle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_create_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.EventPool.pfnCreateCb =
        [](ze_event_pool_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_create_args.hContext1, *params->phContext);
            EXPECT_EQ(&event_pool_create_args.desc1, *params->pdesc);
            EXPECT_EQ(event_pool_create_args.numDevices1, *params->pnumDevices);
            EXPECT_EQ(event_pool_create_args.hDevices1, *params->pphDevices);
            for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
                EXPECT_EQ(event_pool_create_args.hDevices1[i], (*(params->pphDevices))[i]);
            }

            ze_event_pool_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEventPool;

            ze_event_pool_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            EXPECT_EQ(&event_pool_create_args.hEventPool1, pHandle);

            ze_event_pool_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(event_pool_create_args.hEventPool1, handle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.EventPool.pfnCreateCb =
        [](ze_event_pool_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_create_args.hContext1, *params->phContext);
            EXPECT_EQ(&event_pool_create_args.desc1, *params->pdesc);
            EXPECT_EQ(event_pool_create_args.numDevices1, *params->pnumDevices);
            EXPECT_EQ(event_pool_create_args.hDevices1, *params->pphDevices);
            for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
                EXPECT_EQ(event_pool_create_args.hDevices1[i], (*(params->pphDevices))[i]);
            }

            ze_event_pool_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEventPool;

            ze_event_pool_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            EXPECT_EQ(&event_pool_create_args.hEventPool1, pHandle);

            ze_event_pool_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(event_pool_create_args.hEventPool1, handle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.EventPool.pfnCreateCb =
        [](ze_event_pool_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_create_args.hContext1, *params->phContext);
            EXPECT_EQ(&event_pool_create_args.desc1, *params->pdesc);
            EXPECT_EQ(event_pool_create_args.numDevices1, *params->pnumDevices);
            EXPECT_EQ(event_pool_create_args.hDevices1, *params->pphDevices);
            for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
                EXPECT_EQ(event_pool_create_args.hDevices1[i], (*(params->pphDevices))[i]);
            }

            ze_event_pool_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEventPool;

            ze_event_pool_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            EXPECT_EQ(&event_pool_create_args.hEventPool1, pHandle);

            ze_event_pool_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(event_pool_create_args.hEventPool1, handle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_create_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.EventPool.pfnCreateCb =
        [](ze_event_pool_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_create_args.hContext1, *params->phContext);
            EXPECT_EQ(&event_pool_create_args.desc1, *params->pdesc);
            EXPECT_EQ(event_pool_create_args.numDevices1, *params->pnumDevices);
            EXPECT_EQ(event_pool_create_args.hDevices1, *params->pphDevices);
            for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
                EXPECT_EQ(event_pool_create_args.hDevices1[i], (*(params->pphDevices))[i]);
            }

            ze_event_pool_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEventPool;

            ze_event_pool_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            EXPECT_EQ(&event_pool_create_args.hEventPool1, pHandle);

            ze_event_pool_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(event_pool_create_args.hEventPool1, handle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_create_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolCreateTracing(event_pool_create_args.hContext0,
                                      &event_pool_create_args.desc0,
                                      event_pool_create_args.numDevices0,
                                      event_pool_create_args.hDevices0,
                                      &event_pool_create_args.hEventPool0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_pool_handle_t hEventPool0;
    ze_event_pool_handle_t hEventPool1;
    void *instanceData0;
    void *instanceData3;
} event_pool_destroy_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventPoolDestroyTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    event_pool_destroy_args.hEventPool0 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize replacement argument set
    event_pool_destroy_args.hEventPool1 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize user instance data
    event_pool_destroy_args.instanceData0 = generateRandomHandle<void *>();
    event_pool_destroy_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.EventPool.pfnDestroy =
        [](ze_event_pool_handle_t hEventPool) {
            EXPECT_EQ(event_pool_destroy_args.hEventPool1, hEventPool);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.EventPool.pfnDestroyCb =
        [](ze_event_pool_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_destroy_args.hEventPool0, *params->phEventPool);
            *params->phEventPool = event_pool_destroy_args.hEventPool1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_pool_destroy_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.EventPool.pfnDestroyCb =
        [](ze_event_pool_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_destroy_args.hEventPool1, *params->phEventPool);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_pool_destroy_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.EventPool.pfnDestroyCb =
        [](ze_event_pool_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_destroy_args.hEventPool1, *params->phEventPool);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.EventPool.pfnDestroyCb =
        [](ze_event_pool_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_destroy_args.hEventPool1, *params->phEventPool);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.EventPool.pfnDestroyCb =
        [](ze_event_pool_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_destroy_args.hEventPool1, *params->phEventPool);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_pool_destroy_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.EventPool.pfnDestroyCb =
        [](ze_event_pool_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_destroy_args.hEventPool1, *params->phEventPool);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_pool_destroy_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolDestroyTracing(event_pool_destroy_args.hEventPool0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_pool_handle_t hEventPool0;
    ze_ipc_event_pool_handle_t hIpc0;
    ze_event_pool_handle_t hEventPool1;
    ze_ipc_event_pool_handle_t hIpc1;
    ze_ipc_event_pool_handle_t hIpcAPI;
    void *instanceData0;
    void *instanceData3;
} event_pool_get_ipc_handle_args;

static void eventPoolGetIpcHandleInitRandom(ze_ipc_event_pool_handle_t *phIpc) {
    uint8_t *ptr = (uint8_t *)phIpc;
    for (size_t i = 0; i < sizeof(*phIpc); i++, ptr++) {
        *ptr = generateRandomSize<uint8_t>();
    }
}

static bool eventPoolGetIpcHandlesCompare(ze_ipc_event_pool_handle_t *phIpc0, ze_ipc_event_pool_handle_t *phIpc1) {
    if (nullptr == phIpc0) {
        return false;
    }
    if (nullptr == phIpc1) {
        return false;
    }
    return (memcmp((void *)phIpc0, (void *)phIpc1, sizeof(ze_ipc_event_pool_handle_t)) == 0);
}

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventPoolGetIpcHandleTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    event_pool_get_ipc_handle_args.hEventPool0 = generateRandomHandle<ze_event_pool_handle_t>();
    eventPoolGetIpcHandleInitRandom(&event_pool_get_ipc_handle_args.hIpc0);

    // initialize replacement argument set
    event_pool_get_ipc_handle_args.hEventPool1 = generateRandomHandle<ze_event_pool_handle_t>();
    eventPoolGetIpcHandleInitRandom(&event_pool_get_ipc_handle_args.hIpc1);

    // initialize user instance data
    event_pool_get_ipc_handle_args.instanceData0 = generateRandomHandle<void *>();
    event_pool_get_ipc_handle_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.EventPool.pfnGetIpcHandle =
        [](ze_event_pool_handle_t hEventPool, ze_ipc_event_pool_handle_t *phIpc) {
            EXPECT_EQ(event_pool_get_ipc_handle_args.hEventPool1, hEventPool);
            EXPECT_EQ(&event_pool_get_ipc_handle_args.hIpc1, phIpc);
            EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&event_pool_get_ipc_handle_args.hIpc1, phIpc));
            eventPoolGetIpcHandleInitRandom(&event_pool_get_ipc_handle_args.hIpcAPI);
            *phIpc = event_pool_get_ipc_handle_args.hIpcAPI;
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.EventPool.pfnGetIpcHandleCb =
        [](ze_event_pool_get_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_get_ipc_handle_args.hEventPool0, *params->phEventPool);
            EXPECT_EQ(&event_pool_get_ipc_handle_args.hIpc0, *params->pphIpc);
            EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&event_pool_get_ipc_handle_args.hIpc0, *params->pphIpc));
            *params->phEventPool = event_pool_get_ipc_handle_args.hEventPool1;
            *params->pphIpc = &event_pool_get_ipc_handle_args.hIpc1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_pool_get_ipc_handle_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.EventPool.pfnGetIpcHandleCb =
        [](ze_event_pool_get_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_get_ipc_handle_args.hEventPool1, *params->phEventPool);
            EXPECT_EQ(&event_pool_get_ipc_handle_args.hIpc1, *params->pphIpc);
            EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&event_pool_get_ipc_handle_args.hIpc1, *params->pphIpc));
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_pool_get_ipc_handle_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.EventPool.pfnGetIpcHandleCb =
        [](ze_event_pool_get_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_get_ipc_handle_args.hEventPool1, *params->phEventPool);
            EXPECT_EQ(&event_pool_get_ipc_handle_args.hIpc1, *params->pphIpc);
            EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&event_pool_get_ipc_handle_args.hIpc1, *params->pphIpc));
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.EventPool.pfnGetIpcHandleCb =
        [](ze_event_pool_get_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_get_ipc_handle_args.hEventPool1, *params->phEventPool);
            EXPECT_EQ(&event_pool_get_ipc_handle_args.hIpc1, *params->pphIpc);
            EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&event_pool_get_ipc_handle_args.hIpc1, *params->pphIpc));
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.EventPool.pfnGetIpcHandleCb =
        [](ze_event_pool_get_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_get_ipc_handle_args.hEventPool1, *params->phEventPool);
            EXPECT_EQ(&event_pool_get_ipc_handle_args.hIpc1, *params->pphIpc);
            EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&event_pool_get_ipc_handle_args.hIpc1, *params->pphIpc));
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_pool_get_ipc_handle_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.EventPool.pfnGetIpcHandleCb =
        [](ze_event_pool_get_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_get_ipc_handle_args.hEventPool1, *params->phEventPool);
            EXPECT_EQ(&event_pool_get_ipc_handle_args.hIpc1, *params->pphIpc);
            EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&event_pool_get_ipc_handle_args.hIpc1, *params->pphIpc));
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_pool_get_ipc_handle_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolGetIpcHandleTracing(event_pool_get_ipc_handle_args.hEventPool0, &event_pool_get_ipc_handle_args.hIpc0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_context_handle_t hContext0;
    ze_ipc_event_pool_handle_t hIpc0;
    ze_event_pool_handle_t hEventPool0;
    ze_context_handle_t hContext1;
    ze_ipc_event_pool_handle_t hIpc1;
    ze_event_pool_handle_t hEventPool1;
    ze_event_pool_handle_t hEventPoolAPI;
    void *instanceData0;
    void *instanceData3;
} event_pool_open_ipc_handle_args;

static void eventPoolOpenIpcHandleInitRandom(ze_ipc_event_pool_handle_t *phIpc) {
    uint8_t *ptr = (uint8_t *)phIpc;
    for (size_t i = 0; i < sizeof(*phIpc); i++, ptr++) {
        *ptr = generateRandomSize<uint8_t>();
    }
}

static bool eventPoolOpenIpcHandlesCompare(ze_ipc_event_pool_handle_t *phIpc0, ze_ipc_event_pool_handle_t *phIpc1) {
    return (memcmp((void *)phIpc0, (void *)phIpc1, sizeof(ze_ipc_event_pool_handle_t)) == 0);
}

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventPoolOpenIpcHandleTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    event_pool_open_ipc_handle_args.hContext0 = generateRandomHandle<ze_context_handle_t>();
    eventPoolOpenIpcHandleInitRandom(&event_pool_open_ipc_handle_args.hIpc0);
    event_pool_open_ipc_handle_args.hEventPool0 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize replacement argument set
    event_pool_open_ipc_handle_args.hContext1 = generateRandomHandle<ze_context_handle_t>();
    eventPoolOpenIpcHandleInitRandom(&event_pool_open_ipc_handle_args.hIpc1);
    event_pool_open_ipc_handle_args.hEventPool1 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize user instance data
    event_pool_open_ipc_handle_args.instanceData0 = generateRandomHandle<void *>();
    event_pool_open_ipc_handle_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.EventPool.pfnOpenIpcHandle =
        [](ze_context_handle_t hContext, ze_ipc_event_pool_handle_t hIpc, ze_event_pool_handle_t *phEventPool) {
            EXPECT_EQ(event_pool_open_ipc_handle_args.hContext1, hContext);
            EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&event_pool_open_ipc_handle_args.hIpc1, &hIpc));
            EXPECT_EQ(event_pool_open_ipc_handle_args.hEventPool1, *phEventPool);
            EXPECT_EQ(&event_pool_open_ipc_handle_args.hEventPool1, phEventPool);
            event_pool_open_ipc_handle_args.hEventPoolAPI = generateRandomHandle<ze_event_pool_handle_t>();
            *phEventPool = event_pool_open_ipc_handle_args.hEventPoolAPI;
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.EventPool.pfnOpenIpcHandleCb =
        [](ze_event_pool_open_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_open_ipc_handle_args.hContext0, *params->phContext);
            EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&event_pool_open_ipc_handle_args.hIpc0, params->phIpc));

            ze_event_pool_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEventPool;

            ze_event_pool_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            EXPECT_EQ(&event_pool_open_ipc_handle_args.hEventPool0, pHandle);

            ze_event_pool_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(event_pool_open_ipc_handle_args.hEventPool0, handle);

            *params->phContext = event_pool_open_ipc_handle_args.hContext1;
            *params->phIpc = event_pool_open_ipc_handle_args.hIpc1;
            *params->pphEventPool = &event_pool_open_ipc_handle_args.hEventPool1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_pool_open_ipc_handle_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.EventPool.pfnOpenIpcHandleCb =
        [](ze_event_pool_open_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_open_ipc_handle_args.hContext1, *params->phContext);
            EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&event_pool_open_ipc_handle_args.hIpc1, params->phIpc));

            ze_event_pool_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEventPool;

            ze_event_pool_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&event_pool_open_ipc_handle_args.hEventPool1, pHandle);

            ze_event_pool_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(event_pool_open_ipc_handle_args.hEventPool1, handle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_pool_open_ipc_handle_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.EventPool.pfnOpenIpcHandleCb =
        [](ze_event_pool_open_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_open_ipc_handle_args.hContext1, *params->phContext);
            EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&event_pool_open_ipc_handle_args.hIpc1, params->phIpc));

            ze_event_pool_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEventPool;

            ze_event_pool_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&event_pool_open_ipc_handle_args.hEventPool1, pHandle);

            ze_event_pool_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(event_pool_open_ipc_handle_args.hEventPool1, handle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.EventPool.pfnOpenIpcHandleCb =
        [](ze_event_pool_open_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_open_ipc_handle_args.hContext1, *params->phContext);
            EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&event_pool_open_ipc_handle_args.hIpc1, params->phIpc));

            ze_event_pool_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEventPool;

            ze_event_pool_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&event_pool_open_ipc_handle_args.hEventPool1, pHandle);

            ze_event_pool_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(event_pool_open_ipc_handle_args.hEventPool1, handle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.EventPool.pfnOpenIpcHandleCb =
        [](ze_event_pool_open_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_open_ipc_handle_args.hContext1, *params->phContext);
            EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&event_pool_open_ipc_handle_args.hIpc1, params->phIpc));

            ze_event_pool_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEventPool;

            ze_event_pool_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&event_pool_open_ipc_handle_args.hEventPool1, pHandle);

            ze_event_pool_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(event_pool_open_ipc_handle_args.hEventPool1, handle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_pool_open_ipc_handle_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.EventPool.pfnOpenIpcHandleCb =
        [](ze_event_pool_open_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_open_ipc_handle_args.hContext1, *params->phContext);
            EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&event_pool_open_ipc_handle_args.hIpc1, params->phIpc));

            ze_event_pool_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphEventPool;

            ze_event_pool_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&event_pool_open_ipc_handle_args.hEventPool1, pHandle);

            ze_event_pool_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(event_pool_open_ipc_handle_args.hEventPool1, handle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_pool_open_ipc_handle_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolOpenIpcHandleTracing(event_pool_open_ipc_handle_args.hContext0,
                                             event_pool_open_ipc_handle_args.hIpc0,
                                             &event_pool_open_ipc_handle_args.hEventPool0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_pool_handle_t hEventPool0;
    ze_event_pool_handle_t hEventPool1;
    void *instanceData0;
    void *instanceData3;
} event_pool_close_ipc_handle_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventPoolCloseIpcHandleTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    event_pool_close_ipc_handle_args.hEventPool0 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize replacement argument set
    event_pool_close_ipc_handle_args.hEventPool1 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize user instance data
    event_pool_close_ipc_handle_args.instanceData0 = generateRandomHandle<void *>();
    event_pool_close_ipc_handle_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.EventPool.pfnCloseIpcHandle =
        [](ze_event_pool_handle_t hEventPool) {
            EXPECT_EQ(event_pool_close_ipc_handle_args.hEventPool1, hEventPool);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.EventPool.pfnCloseIpcHandleCb =
        [](ze_event_pool_close_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_close_ipc_handle_args.hEventPool0, *params->phEventPool);
            *params->phEventPool = event_pool_close_ipc_handle_args.hEventPool1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_pool_close_ipc_handle_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.EventPool.pfnCloseIpcHandleCb =
        [](ze_event_pool_close_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_close_ipc_handle_args.hEventPool1, *params->phEventPool);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_pool_close_ipc_handle_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.EventPool.pfnCloseIpcHandleCb =
        [](ze_event_pool_close_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_close_ipc_handle_args.hEventPool1, *params->phEventPool);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.EventPool.pfnCloseIpcHandleCb =
        [](ze_event_pool_close_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_close_ipc_handle_args.hEventPool1, *params->phEventPool);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.EventPool.pfnCloseIpcHandleCb =
        [](ze_event_pool_close_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(event_pool_close_ipc_handle_args.hEventPool1, *params->phEventPool);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = event_pool_close_ipc_handle_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.EventPool.pfnCloseIpcHandleCb =
        [](ze_event_pool_close_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(event_pool_close_ipc_handle_args.hEventPool1, *params->phEventPool);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, event_pool_close_ipc_handle_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolCloseIpcHandleTracing(event_pool_close_ipc_handle_args.hEventPool0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

// Command List API with Events

struct {
    ze_command_list_handle_t hCommandList0;
    ze_event_handle_t hEvent0;
    ze_command_list_handle_t hCommandList1;
    ze_event_handle_t hEvent1;
    void *instanceData0;
    void *instanceData3;
} command_list_append_signal_event_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingCommandListAppendSignalEventTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    command_list_append_signal_event_args.hCommandList0 = generateRandomHandle<ze_command_list_handle_t>();
    command_list_append_signal_event_args.hEvent0 = generateRandomHandle<ze_event_handle_t>();

    // initialize replacement argument set
    command_list_append_signal_event_args.hCommandList1 = generateRandomHandle<ze_command_list_handle_t>();
    command_list_append_signal_event_args.hEvent1 = generateRandomHandle<ze_event_handle_t>();

    // initialize user instance data
    command_list_append_signal_event_args.instanceData0 = generateRandomHandle<void *>();
    command_list_append_signal_event_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.CommandList.pfnAppendSignalEvent =
        [](ze_command_list_handle_t hCommandList, ze_event_handle_t hEvent) {
            EXPECT_EQ(command_list_append_signal_event_args.hCommandList1, hCommandList);
            EXPECT_EQ(command_list_append_signal_event_args.hEvent1, hEvent);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.CommandList.pfnAppendSignalEventCb =
        [](ze_command_list_append_signal_event_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(command_list_append_signal_event_args.hCommandList0, *params->phCommandList);
            EXPECT_EQ(command_list_append_signal_event_args.hEvent0, *params->phEvent);
            *params->phCommandList = command_list_append_signal_event_args.hCommandList1;
            *params->phEvent = command_list_append_signal_event_args.hEvent1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = command_list_append_signal_event_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.CommandList.pfnAppendSignalEventCb =
        [](ze_command_list_append_signal_event_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(command_list_append_signal_event_args.hCommandList1, *params->phCommandList);
            EXPECT_EQ(command_list_append_signal_event_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, command_list_append_signal_event_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.CommandList.pfnAppendSignalEventCb =
        [](ze_command_list_append_signal_event_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(command_list_append_signal_event_args.hCommandList1, *params->phCommandList);
            EXPECT_EQ(command_list_append_signal_event_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.CommandList.pfnAppendSignalEventCb =
        [](ze_command_list_append_signal_event_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(command_list_append_signal_event_args.hCommandList1, *params->phCommandList);
            EXPECT_EQ(command_list_append_signal_event_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Allocate instance data and pass to corresponding epilog
    //
    prologCbs3.CommandList.pfnAppendSignalEventCb =
        [](ze_command_list_append_signal_event_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(command_list_append_signal_event_args.hCommandList1, *params->phCommandList);
            EXPECT_EQ(command_list_append_signal_event_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = command_list_append_signal_event_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    epilogCbs3.CommandList.pfnAppendSignalEventCb =
        [](ze_command_list_append_signal_event_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(command_list_append_signal_event_args.hCommandList1, *params->phCommandList);
            EXPECT_EQ(command_list_append_signal_event_args.hEvent1, *params->phEvent);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, command_list_append_signal_event_args.instanceData0);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendSignalEventTracing(command_list_append_signal_event_args.hCommandList0, command_list_append_signal_event_args.hEvent0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

#define NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_0 2
#define NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1 4

struct {
    ze_command_list_handle_t hCommandList0;
    uint32_t numEvents0 = NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_0;
    ze_event_handle_t hEvents0[NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_0];
    ze_command_list_handle_t hCommandList1;
    uint32_t numEvents1 = NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1;
    ze_event_handle_t hEvents1[NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1];
    void *instanceData0;
    void *instanceData3;
} command_list_append_wait_on_events_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingCommandListAppendWaitOnEventsTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    command_list_append_wait_on_events_args.hCommandList0 = generateRandomHandle<ze_command_list_handle_t>();
    for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_0; i++) {
        command_list_append_wait_on_events_args.hEvents0[i] = generateRandomHandle<ze_event_handle_t>();
    }

    // initialize replacement argument set
    command_list_append_wait_on_events_args.hCommandList1 = generateRandomHandle<ze_command_list_handle_t>();
    for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
        command_list_append_wait_on_events_args.hEvents1[i] = generateRandomHandle<ze_event_handle_t>();
    }

    // initialize user instance data
    command_list_append_wait_on_events_args.instanceData0 = generateRandomHandle<void *>();
    command_list_append_wait_on_events_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.CommandList.pfnAppendWaitOnEvents =
        [](ze_command_list_handle_t hCommandList, uint32_t numEvents, ze_event_handle_t *phEvents) {
            EXPECT_EQ(command_list_append_wait_on_events_args.hCommandList1, hCommandList);
            EXPECT_EQ(numEvents, (uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1);
            for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
                EXPECT_EQ(command_list_append_wait_on_events_args.hEvents1[i], phEvents[i]);
            }
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.CommandList.pfnAppendWaitOnEventsCb =
        [](ze_command_list_append_wait_on_events_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(command_list_append_wait_on_events_args.hCommandList0, *params->phCommandList);
            EXPECT_EQ((uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_0, *params->pnumEvents);
            for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_0; i++) {
                EXPECT_EQ(command_list_append_wait_on_events_args.hEvents0[i], (*(params->pphEvents))[i]);
            }
            *params->phCommandList = command_list_append_wait_on_events_args.hCommandList1;
            *params->pnumEvents = NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1;
            *params->pphEvents = command_list_append_wait_on_events_args.hEvents1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = command_list_append_wait_on_events_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.CommandList.pfnAppendWaitOnEventsCb =
        [](ze_command_list_append_wait_on_events_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(command_list_append_wait_on_events_args.hCommandList1, *params->phCommandList);
            EXPECT_EQ((uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1, *params->pnumEvents);
            for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
                EXPECT_EQ(command_list_append_wait_on_events_args.hEvents1[i], (*(params->pphEvents))[i]);
            }
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, command_list_append_wait_on_events_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.CommandList.pfnAppendWaitOnEventsCb =
        [](ze_command_list_append_wait_on_events_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(command_list_append_wait_on_events_args.hCommandList1, *params->phCommandList);
            EXPECT_EQ((uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1, *params->pnumEvents);
            for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
                EXPECT_EQ(command_list_append_wait_on_events_args.hEvents1[i], (*(params->pphEvents))[i]);
            }
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.CommandList.pfnAppendWaitOnEventsCb =
        [](ze_command_list_append_wait_on_events_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(command_list_append_wait_on_events_args.hCommandList1, *params->phCommandList);
            EXPECT_EQ((uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1, *params->pnumEvents);
            for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
                EXPECT_EQ(command_list_append_wait_on_events_args.hEvents1[i], (*(params->pphEvents))[i]);
            }
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Allocate instance data and pass to corresponding epilog
    //
    prologCbs3.CommandList.pfnAppendWaitOnEventsCb =
        [](ze_command_list_append_wait_on_events_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(command_list_append_wait_on_events_args.hCommandList1, *params->phCommandList);
            EXPECT_EQ((uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1, *params->pnumEvents);
            for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
                EXPECT_EQ(command_list_append_wait_on_events_args.hEvents1[i], (*(params->pphEvents))[i]);
            }
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = command_list_append_wait_on_events_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    epilogCbs3.CommandList.pfnAppendWaitOnEventsCb =
        [](ze_command_list_append_wait_on_events_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(command_list_append_wait_on_events_args.hCommandList1, *params->phCommandList);
            EXPECT_EQ((uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1, *params->pnumEvents);
            for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
                EXPECT_EQ(command_list_append_wait_on_events_args.hEvents1[i], (*(params->pphEvents))[i]);
            }
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, command_list_append_wait_on_events_args.instanceData0);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendWaitOnEventsTracing(command_list_append_wait_on_events_args.hCommandList0,
                                                    NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_0,
                                                    command_list_append_wait_on_events_args.hEvents0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

} // namespace ult
} // namespace L0
