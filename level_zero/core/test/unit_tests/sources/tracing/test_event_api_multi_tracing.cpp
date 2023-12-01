/*
 * Copyright (C) 2020-2023 Intel Corporation
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
} eventCreateArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventCreateTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    eventCreateArgs.hEventPool0 = generateRandomHandle<ze_event_pool_handle_t>();
    eventCreateArgs.hEvent0 = generateRandomHandle<ze_event_handle_t>();

    // initialize replacement argument set
    eventCreateArgs.hEventPool1 = generateRandomHandle<ze_event_pool_handle_t>();
    eventCreateArgs.hEvent1 = generateRandomHandle<ze_event_handle_t>();

    // initialize user instance data
    eventCreateArgs.instanceData0 = generateRandomHandle<void *>();
    eventCreateArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Event.pfnCreate =
        [](ze_event_pool_handle_t hEventPool, const ze_event_desc_t *desc, ze_event_handle_t *phEvent) -> ze_result_t {
        EXPECT_EQ(eventCreateArgs.hEventPool1, hEventPool);
        EXPECT_EQ(&eventCreateArgs.desc1, desc);
        EXPECT_EQ(&eventCreateArgs.hEvent1, phEvent);
        EXPECT_EQ(eventCreateArgs.hEvent1, *phEvent);
        eventCreateArgs.hEventAPI = generateRandomHandle<ze_event_handle_t>();
        *phEvent = eventCreateArgs.hEventAPI;
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Event.pfnCreateCb =
        [](ze_event_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventCreateArgs.hEventPool0, *params->phEventPool);
        EXPECT_EQ(&eventCreateArgs.desc0, *params->pdesc);
        EXPECT_EQ(&eventCreateArgs.hEvent0, *params->pphEvent);

        ze_event_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEvent;

        ze_event_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_event_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(eventCreateArgs.hEvent0, handle);
        *params->phEventPool = eventCreateArgs.hEventPool1;
        *params->pdesc = &eventCreateArgs.desc1;
        *params->pphEvent = &eventCreateArgs.hEvent1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventCreateArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Event.pfnCreateCb =
        [](ze_event_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventCreateArgs.hEventPool1, *params->phEventPool);
        EXPECT_EQ(&eventCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&eventCreateArgs.hEvent1, *params->pphEvent);

        ze_event_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEvent;

        ze_event_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_event_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(eventCreateArgs.hEvent1, handle);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventCreateArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Event.pfnCreateCb =
        [](ze_event_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventCreateArgs.hEventPool1, *params->phEventPool);
        EXPECT_EQ(&eventCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&eventCreateArgs.hEvent1, *params->pphEvent);

        ze_event_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEvent;

        ze_event_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_event_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(eventCreateArgs.hEvent1, handle);
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
        [](ze_event_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventCreateArgs.hEventPool1, *params->phEventPool);
        EXPECT_EQ(&eventCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&eventCreateArgs.hEvent1, *params->pphEvent);

        ze_event_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEvent;

        ze_event_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_event_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(eventCreateArgs.hEvent1, handle);
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
        [](ze_event_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventCreateArgs.hEventPool1, *params->phEventPool);
        EXPECT_EQ(&eventCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&eventCreateArgs.hEvent1, *params->pphEvent);

        ze_event_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEvent;

        ze_event_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_event_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(eventCreateArgs.hEvent1, handle);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventCreateArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Event.pfnCreateCb =
        [](ze_event_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventCreateArgs.hEventPool1, *params->phEventPool);
        EXPECT_EQ(&eventCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&eventCreateArgs.hEvent1, *params->pphEvent);

        ze_event_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEvent;

        ze_event_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_event_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(eventCreateArgs.hEvent1, handle);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventCreateArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeEventCreateTracing(eventCreateArgs.hEventPool0, &eventCreateArgs.desc0, &eventCreateArgs.hEvent0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_handle_t hEvent0;
    ze_event_handle_t hEvent1;
    void *instanceData0;
    void *instanceData3;
} eventDestroyArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventDestroyTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    eventDestroyArgs.hEvent0 = generateRandomHandle<ze_event_handle_t>();

    // initialize replacement argument set
    eventDestroyArgs.hEvent1 = generateRandomHandle<ze_event_handle_t>();

    // initialize user instance data
    eventDestroyArgs.instanceData0 = generateRandomHandle<void *>();
    eventDestroyArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Event.pfnDestroy =
        [](ze_event_handle_t hEvent) -> ze_result_t {
        EXPECT_EQ(eventDestroyArgs.hEvent1, hEvent);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Event.pfnDestroyCb =
        [](ze_event_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventDestroyArgs.hEvent0, *params->phEvent);
        *params->phEvent = eventDestroyArgs.hEvent1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventDestroyArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Event.pfnDestroyCb =
        [](ze_event_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventDestroyArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventDestroyArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Event.pfnDestroyCb =
        [](ze_event_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventDestroyArgs.hEvent1, *params->phEvent);
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
        [](ze_event_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventDestroyArgs.hEvent1, *params->phEvent);
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
        [](ze_event_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventDestroyArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventDestroyArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Event.pfnDestroyCb =
        [](ze_event_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventDestroyArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventDestroyArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeEventDestroyTracing(eventDestroyArgs.hEvent0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_handle_t hEvent0;
    ze_event_handle_t hEvent1;
    void *instanceData0;
    void *instanceData3;
} eventHostSignalArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventHostSignalTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    eventHostSignalArgs.hEvent0 = generateRandomHandle<ze_event_handle_t>();

    // initialize replacement argument set
    eventHostSignalArgs.hEvent1 = generateRandomHandle<ze_event_handle_t>();

    // initialize user instance data
    eventHostSignalArgs.instanceData0 = generateRandomHandle<void *>();
    eventHostSignalArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Event.pfnHostSignal =
        [](ze_event_handle_t hEvent) -> ze_result_t {
        EXPECT_EQ(eventHostSignalArgs.hEvent1, hEvent);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Event.pfnHostSignalCb =
        [](ze_event_host_signal_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventHostSignalArgs.hEvent0, *params->phEvent);
        *params->phEvent = eventHostSignalArgs.hEvent1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventHostSignalArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Event.pfnHostSignalCb =
        [](ze_event_host_signal_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventHostSignalArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventHostSignalArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Event.pfnHostSignalCb =
        [](ze_event_host_signal_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventHostSignalArgs.hEvent1, *params->phEvent);
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
        [](ze_event_host_signal_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventHostSignalArgs.hEvent1, *params->phEvent);
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
        [](ze_event_host_signal_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventHostSignalArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventHostSignalArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Event.pfnHostSignalCb =
        [](ze_event_host_signal_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventHostSignalArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventHostSignalArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeEventHostSignalTracing(eventHostSignalArgs.hEvent0);
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
} eventHostSynchronizeArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventHostSynchronizeTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    eventHostSynchronizeArgs.hEvent0 = generateRandomHandle<ze_event_handle_t>();
    eventHostSynchronizeArgs.timeout0 = generateRandomSize<uint32_t>();

    // initialize replacement argument set
    eventHostSynchronizeArgs.hEvent1 = generateRandomHandle<ze_event_handle_t>();
    eventHostSynchronizeArgs.timeout1 = generateRandomSize<uint32_t>();

    // initialize user instance data
    eventHostSynchronizeArgs.instanceData0 = generateRandomHandle<void *>();
    eventHostSynchronizeArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Event.pfnHostSynchronize =
        [](ze_event_handle_t hEvent, uint64_t timeout) -> ze_result_t {
        EXPECT_EQ(eventHostSynchronizeArgs.hEvent1, hEvent);
        EXPECT_EQ(eventHostSynchronizeArgs.timeout1, timeout);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Event.pfnHostSynchronizeCb =
        [](ze_event_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventHostSynchronizeArgs.hEvent0, *params->phEvent);
        EXPECT_EQ(eventHostSynchronizeArgs.timeout0, *params->ptimeout);
        *params->phEvent = eventHostSynchronizeArgs.hEvent1;
        *params->ptimeout = eventHostSynchronizeArgs.timeout1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventHostSynchronizeArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Event.pfnHostSynchronizeCb =
        [](ze_event_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventHostSynchronizeArgs.hEvent1, *params->phEvent);
        EXPECT_EQ(eventHostSynchronizeArgs.timeout1, *params->ptimeout);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventHostSynchronizeArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Event.pfnHostSynchronizeCb =
        [](ze_event_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventHostSynchronizeArgs.hEvent1, *params->phEvent);
        EXPECT_EQ(eventHostSynchronizeArgs.timeout1, *params->ptimeout);
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
        [](ze_event_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        ASSERT_NE(nullptr, pTracerUserData);
        EXPECT_EQ(eventHostSynchronizeArgs.hEvent1, *params->phEvent);
        EXPECT_EQ(eventHostSynchronizeArgs.timeout1, *params->ptimeout);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 21);
        *val += 21;
    };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Event.pfnHostSynchronizeCb =
        [](ze_event_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventHostSynchronizeArgs.hEvent1, *params->phEvent);
        EXPECT_EQ(eventHostSynchronizeArgs.timeout1, *params->ptimeout);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventHostSynchronizeArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Event.pfnHostSynchronizeCb =
        [](ze_event_host_synchronize_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventHostSynchronizeArgs.hEvent1, *params->phEvent);
        EXPECT_EQ(eventHostSynchronizeArgs.timeout1, *params->ptimeout);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventHostSynchronizeArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeEventHostSynchronizeTracing(eventHostSynchronizeArgs.hEvent0, eventHostSynchronizeArgs.timeout0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_handle_t hEvent0;
    ze_event_handle_t hEvent1;
    void *instanceData0;
    void *instanceData3;
} eventQueryStatusArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventQueryStatusTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    eventQueryStatusArgs.hEvent0 = generateRandomHandle<ze_event_handle_t>();

    // initialize replacement argument set
    eventQueryStatusArgs.hEvent1 = generateRandomHandle<ze_event_handle_t>();

    // initialize user instance data
    eventQueryStatusArgs.instanceData0 = generateRandomHandle<void *>();
    eventQueryStatusArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Event.pfnQueryStatus =
        [](ze_event_handle_t hEvent) -> ze_result_t {
        EXPECT_EQ(eventQueryStatusArgs.hEvent1, hEvent);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Event.pfnQueryStatusCb =
        [](ze_event_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventQueryStatusArgs.hEvent0, *params->phEvent);
        *params->phEvent = eventQueryStatusArgs.hEvent1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventQueryStatusArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Event.pfnQueryStatusCb =
        [](ze_event_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventQueryStatusArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventQueryStatusArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Event.pfnQueryStatusCb =
        [](ze_event_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventQueryStatusArgs.hEvent1, *params->phEvent);
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
        [](ze_event_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        ASSERT_NE(nullptr, pTracerUserData);
        EXPECT_EQ(eventQueryStatusArgs.hEvent1, *params->phEvent);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 21);
        *val += 21;
    };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Event.pfnQueryStatusCb =
        [](ze_event_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventQueryStatusArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventQueryStatusArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Event.pfnQueryStatusCb =
        [](ze_event_query_status_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventQueryStatusArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventQueryStatusArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeEventQueryStatusTracing(eventQueryStatusArgs.hEvent0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_handle_t hEvent0;
    ze_event_handle_t hEvent1;
    void *instanceData0;
    void *instanceData3;
} eventResetArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventHostResetTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    eventResetArgs.hEvent0 = generateRandomHandle<ze_event_handle_t>();

    // initialize replacement argument set
    eventResetArgs.hEvent1 = generateRandomHandle<ze_event_handle_t>();

    // initialize user instance data
    eventResetArgs.instanceData0 = generateRandomHandle<void *>();
    eventResetArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Event.pfnHostReset =
        [](ze_event_handle_t hEvent) -> ze_result_t {
        EXPECT_EQ(eventResetArgs.hEvent1, hEvent);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Event.pfnHostResetCb =
        [](ze_event_host_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventResetArgs.hEvent0, *params->phEvent);
        *params->phEvent = eventResetArgs.hEvent1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventResetArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Event.pfnHostResetCb =
        [](ze_event_host_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventResetArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventResetArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Event.pfnHostResetCb =
        [](ze_event_host_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventResetArgs.hEvent1, *params->phEvent);
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
        [](ze_event_host_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        ASSERT_NE(nullptr, pTracerUserData);
        EXPECT_EQ(eventResetArgs.hEvent1, *params->phEvent);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 21);
        *val += 21;
    };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Event.pfnHostResetCb =
        [](ze_event_host_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventResetArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventResetArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Event.pfnHostResetCb =
        [](ze_event_host_reset_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventResetArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventResetArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeEventHostResetTracing(eventResetArgs.hEvent0);
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
} eventPoolCreateArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventPoolCreateTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    eventPoolCreateArgs.hContext0 = generateRandomHandle<ze_context_handle_t>();
    for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
        eventPoolCreateArgs.hDevices0[i] = generateRandomHandle<ze_device_handle_t>();
    }
    eventPoolCreateArgs.hEventPool0 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize replacement argument set
    eventPoolCreateArgs.hContext1 = generateRandomHandle<ze_context_handle_t>();
    for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_1; i++) {
        eventPoolCreateArgs.hDevices1[i] = generateRandomHandle<ze_device_handle_t>();
    }
    eventPoolCreateArgs.hEventPool1 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize user instance data
    eventPoolCreateArgs.instanceData0 = generateRandomHandle<void *>();
    eventPoolCreateArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.EventPool.pfnCreate =
        [](ze_context_handle_t hContext, const ze_event_pool_desc_t *desc, uint32_t numDevices, ze_device_handle_t *phDevices, ze_event_pool_handle_t *phEventPool) -> ze_result_t {
        EXPECT_EQ(eventPoolCreateArgs.hContext1, hContext);
        EXPECT_EQ(&eventPoolCreateArgs.desc1, desc);
        EXPECT_EQ(eventPoolCreateArgs.numDevices1, numDevices);
        EXPECT_EQ(eventPoolCreateArgs.hDevices1, phDevices);
        for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_1; i++) {
            EXPECT_EQ(eventPoolCreateArgs.hDevices1[i], phDevices[i]);
        }
        EXPECT_EQ(eventPoolCreateArgs.hEventPool1, *phEventPool);
        EXPECT_EQ(&eventPoolCreateArgs.hEventPool1, phEventPool);
        eventPoolCreateArgs.hEventPoolAPI = generateRandomHandle<ze_event_pool_handle_t>();
        *phEventPool = eventPoolCreateArgs.hEventPoolAPI;
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.EventPool.pfnCreateCb =
        [](ze_event_pool_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolCreateArgs.hContext0, *params->phContext);
        EXPECT_EQ(&eventPoolCreateArgs.desc0, *params->pdesc);
        EXPECT_EQ(eventPoolCreateArgs.numDevices0, *params->pnumDevices);
        EXPECT_EQ(eventPoolCreateArgs.hDevices0, *params->pphDevices);
        for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
            EXPECT_EQ(eventPoolCreateArgs.hDevices0[i], (*(params->pphDevices))[i]);
        }

        ze_event_pool_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEventPool;

        ze_event_pool_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        EXPECT_EQ(&eventPoolCreateArgs.hEventPool0, pHandle);

        ze_event_pool_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(eventPoolCreateArgs.hEventPool0, handle);

        *params->phContext = eventPoolCreateArgs.hContext1;
        *params->pdesc = &eventPoolCreateArgs.desc1;
        *params->pnumDevices = eventPoolCreateArgs.numDevices1;
        *params->pphDevices = eventPoolCreateArgs.hDevices1;
        *params->pphEventPool = &eventPoolCreateArgs.hEventPool1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventCreateArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.EventPool.pfnCreateCb =
        [](ze_event_pool_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(&eventPoolCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(eventPoolCreateArgs.numDevices1, *params->pnumDevices);
        EXPECT_EQ(eventPoolCreateArgs.hDevices1, *params->pphDevices);
        for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
            EXPECT_EQ(eventPoolCreateArgs.hDevices1[i], (*(params->pphDevices))[i]);
        }

        ze_event_pool_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEventPool;

        ze_event_pool_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        EXPECT_EQ(&eventPoolCreateArgs.hEventPool1, pHandle);

        ze_event_pool_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(eventPoolCreateArgs.hEventPool1, handle);

        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventCreateArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.EventPool.pfnCreateCb =
        [](ze_event_pool_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(&eventPoolCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(eventPoolCreateArgs.numDevices1, *params->pnumDevices);
        EXPECT_EQ(eventPoolCreateArgs.hDevices1, *params->pphDevices);
        for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
            EXPECT_EQ(eventPoolCreateArgs.hDevices1[i], (*(params->pphDevices))[i]);
        }

        ze_event_pool_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEventPool;

        ze_event_pool_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        EXPECT_EQ(&eventPoolCreateArgs.hEventPool1, pHandle);

        ze_event_pool_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(eventPoolCreateArgs.hEventPool1, handle);

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
        [](ze_event_pool_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(&eventPoolCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(eventPoolCreateArgs.numDevices1, *params->pnumDevices);
        EXPECT_EQ(eventPoolCreateArgs.hDevices1, *params->pphDevices);
        for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
            EXPECT_EQ(eventPoolCreateArgs.hDevices1[i], (*(params->pphDevices))[i]);
        }

        ze_event_pool_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEventPool;

        ze_event_pool_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        EXPECT_EQ(&eventPoolCreateArgs.hEventPool1, pHandle);

        ze_event_pool_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(eventPoolCreateArgs.hEventPool1, handle);

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
        [](ze_event_pool_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(&eventPoolCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(eventPoolCreateArgs.numDevices1, *params->pnumDevices);
        EXPECT_EQ(eventPoolCreateArgs.hDevices1, *params->pphDevices);
        for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
            EXPECT_EQ(eventPoolCreateArgs.hDevices1[i], (*(params->pphDevices))[i]);
        }

        ze_event_pool_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEventPool;

        ze_event_pool_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        EXPECT_EQ(&eventPoolCreateArgs.hEventPool1, pHandle);

        ze_event_pool_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(eventPoolCreateArgs.hEventPool1, handle);

        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventCreateArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.EventPool.pfnCreateCb =
        [](ze_event_pool_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(&eventPoolCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(eventPoolCreateArgs.numDevices1, *params->pnumDevices);
        EXPECT_EQ(eventPoolCreateArgs.hDevices1, *params->pphDevices);
        for (int i = 0; i < NUM_EVENT_POOL_CREATE_DEVICES_0; i++) {
            EXPECT_EQ(eventPoolCreateArgs.hDevices1[i], (*(params->pphDevices))[i]);
        }

        ze_event_pool_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEventPool;

        ze_event_pool_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        EXPECT_EQ(&eventPoolCreateArgs.hEventPool1, pHandle);

        ze_event_pool_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(eventPoolCreateArgs.hEventPool1, handle);

        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventCreateArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolCreateTracing(eventPoolCreateArgs.hContext0,
                                      &eventPoolCreateArgs.desc0,
                                      eventPoolCreateArgs.numDevices0,
                                      eventPoolCreateArgs.hDevices0,
                                      &eventPoolCreateArgs.hEventPool0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_pool_handle_t hEventPool0;
    ze_event_pool_handle_t hEventPool1;
    void *instanceData0;
    void *instanceData3;
} eventPoolDestroyArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventPoolDestroyTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    eventPoolDestroyArgs.hEventPool0 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize replacement argument set
    eventPoolDestroyArgs.hEventPool1 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize user instance data
    eventPoolDestroyArgs.instanceData0 = generateRandomHandle<void *>();
    eventPoolDestroyArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.EventPool.pfnDestroy =
        [](ze_event_pool_handle_t hEventPool) -> ze_result_t {
        EXPECT_EQ(eventPoolDestroyArgs.hEventPool1, hEventPool);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.EventPool.pfnDestroyCb =
        [](ze_event_pool_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolDestroyArgs.hEventPool0, *params->phEventPool);
        *params->phEventPool = eventPoolDestroyArgs.hEventPool1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventPoolDestroyArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.EventPool.pfnDestroyCb =
        [](ze_event_pool_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolDestroyArgs.hEventPool1, *params->phEventPool);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventPoolDestroyArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.EventPool.pfnDestroyCb =
        [](ze_event_pool_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolDestroyArgs.hEventPool1, *params->phEventPool);
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
        [](ze_event_pool_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolDestroyArgs.hEventPool1, *params->phEventPool);
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
        [](ze_event_pool_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolDestroyArgs.hEventPool1, *params->phEventPool);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventPoolDestroyArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.EventPool.pfnDestroyCb =
        [](ze_event_pool_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolDestroyArgs.hEventPool1, *params->phEventPool);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventPoolDestroyArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolDestroyTracing(eventPoolDestroyArgs.hEventPool0);
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
} eventPoolGetIpcHandleArgs;

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
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    eventPoolGetIpcHandleArgs.hEventPool0 = generateRandomHandle<ze_event_pool_handle_t>();
    eventPoolGetIpcHandleInitRandom(&eventPoolGetIpcHandleArgs.hIpc0);

    // initialize replacement argument set
    eventPoolGetIpcHandleArgs.hEventPool1 = generateRandomHandle<ze_event_pool_handle_t>();
    eventPoolGetIpcHandleInitRandom(&eventPoolGetIpcHandleArgs.hIpc1);

    // initialize user instance data
    eventPoolGetIpcHandleArgs.instanceData0 = generateRandomHandle<void *>();
    eventPoolGetIpcHandleArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.EventPool.pfnGetIpcHandle =
        [](ze_event_pool_handle_t hEventPool, ze_ipc_event_pool_handle_t *phIpc) -> ze_result_t {
        EXPECT_EQ(eventPoolGetIpcHandleArgs.hEventPool1, hEventPool);
        EXPECT_EQ(&eventPoolGetIpcHandleArgs.hIpc1, phIpc);
        EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&eventPoolGetIpcHandleArgs.hIpc1, phIpc));
        eventPoolGetIpcHandleInitRandom(&eventPoolGetIpcHandleArgs.hIpcAPI);
        *phIpc = eventPoolGetIpcHandleArgs.hIpcAPI;
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.EventPool.pfnGetIpcHandleCb =
        [](ze_event_pool_get_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolGetIpcHandleArgs.hEventPool0, *params->phEventPool);
        EXPECT_EQ(&eventPoolGetIpcHandleArgs.hIpc0, *params->pphIpc);
        EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&eventPoolGetIpcHandleArgs.hIpc0, *params->pphIpc));
        *params->phEventPool = eventPoolGetIpcHandleArgs.hEventPool1;
        *params->pphIpc = &eventPoolGetIpcHandleArgs.hIpc1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventPoolGetIpcHandleArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.EventPool.pfnGetIpcHandleCb =
        [](ze_event_pool_get_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolGetIpcHandleArgs.hEventPool1, *params->phEventPool);
        EXPECT_EQ(&eventPoolGetIpcHandleArgs.hIpc1, *params->pphIpc);
        EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&eventPoolGetIpcHandleArgs.hIpc1, *params->pphIpc));
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventPoolGetIpcHandleArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.EventPool.pfnGetIpcHandleCb =
        [](ze_event_pool_get_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolGetIpcHandleArgs.hEventPool1, *params->phEventPool);
        EXPECT_EQ(&eventPoolGetIpcHandleArgs.hIpc1, *params->pphIpc);
        EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&eventPoolGetIpcHandleArgs.hIpc1, *params->pphIpc));
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
        [](ze_event_pool_get_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolGetIpcHandleArgs.hEventPool1, *params->phEventPool);
        EXPECT_EQ(&eventPoolGetIpcHandleArgs.hIpc1, *params->pphIpc);
        EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&eventPoolGetIpcHandleArgs.hIpc1, *params->pphIpc));
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
        [](ze_event_pool_get_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolGetIpcHandleArgs.hEventPool1, *params->phEventPool);
        EXPECT_EQ(&eventPoolGetIpcHandleArgs.hIpc1, *params->pphIpc);
        EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&eventPoolGetIpcHandleArgs.hIpc1, *params->pphIpc));
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventPoolGetIpcHandleArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.EventPool.pfnGetIpcHandleCb =
        [](ze_event_pool_get_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolGetIpcHandleArgs.hEventPool1, *params->phEventPool);
        EXPECT_EQ(&eventPoolGetIpcHandleArgs.hIpc1, *params->pphIpc);
        EXPECT_TRUE(eventPoolGetIpcHandlesCompare(&eventPoolGetIpcHandleArgs.hIpc1, *params->pphIpc));
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventPoolGetIpcHandleArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolGetIpcHandleTracing(eventPoolGetIpcHandleArgs.hEventPool0, &eventPoolGetIpcHandleArgs.hIpc0);
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
} eventPoolOpenIpcHandleArgs;

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
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    eventPoolOpenIpcHandleArgs.hContext0 = generateRandomHandle<ze_context_handle_t>();
    eventPoolOpenIpcHandleInitRandom(&eventPoolOpenIpcHandleArgs.hIpc0);
    eventPoolOpenIpcHandleArgs.hEventPool0 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize replacement argument set
    eventPoolOpenIpcHandleArgs.hContext1 = generateRandomHandle<ze_context_handle_t>();
    eventPoolOpenIpcHandleInitRandom(&eventPoolOpenIpcHandleArgs.hIpc1);
    eventPoolOpenIpcHandleArgs.hEventPool1 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize user instance data
    eventPoolOpenIpcHandleArgs.instanceData0 = generateRandomHandle<void *>();
    eventPoolOpenIpcHandleArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.EventPool.pfnOpenIpcHandle =
        [](ze_context_handle_t hContext, ze_ipc_event_pool_handle_t hIpc, ze_event_pool_handle_t *phEventPool) -> ze_result_t {
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hContext1, hContext);
        EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&eventPoolOpenIpcHandleArgs.hIpc1, &hIpc));
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hEventPool1, *phEventPool);
        EXPECT_EQ(&eventPoolOpenIpcHandleArgs.hEventPool1, phEventPool);
        eventPoolOpenIpcHandleArgs.hEventPoolAPI = generateRandomHandle<ze_event_pool_handle_t>();
        *phEventPool = eventPoolOpenIpcHandleArgs.hEventPoolAPI;
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.EventPool.pfnOpenIpcHandleCb =
        [](ze_event_pool_open_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hContext0, *params->phContext);
        EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&eventPoolOpenIpcHandleArgs.hIpc0, params->phIpc));

        ze_event_pool_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEventPool;

        ze_event_pool_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        EXPECT_EQ(&eventPoolOpenIpcHandleArgs.hEventPool0, pHandle);

        ze_event_pool_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hEventPool0, handle);

        *params->phContext = eventPoolOpenIpcHandleArgs.hContext1;
        *params->phIpc = eventPoolOpenIpcHandleArgs.hIpc1;
        *params->pphEventPool = &eventPoolOpenIpcHandleArgs.hEventPool1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventPoolOpenIpcHandleArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.EventPool.pfnOpenIpcHandleCb =
        [](ze_event_pool_open_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hContext1, *params->phContext);
        EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&eventPoolOpenIpcHandleArgs.hIpc1, params->phIpc));

        ze_event_pool_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEventPool;

        ze_event_pool_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;
        EXPECT_EQ(&eventPoolOpenIpcHandleArgs.hEventPool1, pHandle);

        ze_event_pool_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hEventPool1, handle);

        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventPoolOpenIpcHandleArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.EventPool.pfnOpenIpcHandleCb =
        [](ze_event_pool_open_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hContext1, *params->phContext);
        EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&eventPoolOpenIpcHandleArgs.hIpc1, params->phIpc));

        ze_event_pool_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEventPool;

        ze_event_pool_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;
        EXPECT_EQ(&eventPoolOpenIpcHandleArgs.hEventPool1, pHandle);

        ze_event_pool_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hEventPool1, handle);

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
        [](ze_event_pool_open_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hContext1, *params->phContext);
        EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&eventPoolOpenIpcHandleArgs.hIpc1, params->phIpc));

        ze_event_pool_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEventPool;

        ze_event_pool_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;
        EXPECT_EQ(&eventPoolOpenIpcHandleArgs.hEventPool1, pHandle);

        ze_event_pool_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hEventPool1, handle);

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
        [](ze_event_pool_open_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hContext1, *params->phContext);
        EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&eventPoolOpenIpcHandleArgs.hIpc1, params->phIpc));

        ze_event_pool_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEventPool;

        ze_event_pool_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;
        EXPECT_EQ(&eventPoolOpenIpcHandleArgs.hEventPool1, pHandle);

        ze_event_pool_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hEventPool1, handle);

        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventPoolOpenIpcHandleArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.EventPool.pfnOpenIpcHandleCb =
        [](ze_event_pool_open_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hContext1, *params->phContext);
        EXPECT_TRUE(eventPoolOpenIpcHandlesCompare(&eventPoolOpenIpcHandleArgs.hIpc1, params->phIpc));

        ze_event_pool_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphEventPool;

        ze_event_pool_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;
        EXPECT_EQ(&eventPoolOpenIpcHandleArgs.hEventPool1, pHandle);

        ze_event_pool_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(eventPoolOpenIpcHandleArgs.hEventPool1, handle);

        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventPoolOpenIpcHandleArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolOpenIpcHandleTracing(eventPoolOpenIpcHandleArgs.hContext0,
                                             eventPoolOpenIpcHandleArgs.hIpc0,
                                             &eventPoolOpenIpcHandleArgs.hEventPool0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_event_pool_handle_t hEventPool0;
    ze_event_pool_handle_t hEventPool1;
    void *instanceData0;
    void *instanceData3;
} eventPoolCloseIpcHandleArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingEventPoolCloseIpcHandleTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    eventPoolCloseIpcHandleArgs.hEventPool0 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize replacement argument set
    eventPoolCloseIpcHandleArgs.hEventPool1 = generateRandomHandle<ze_event_pool_handle_t>();

    // initialize user instance data
    eventPoolCloseIpcHandleArgs.instanceData0 = generateRandomHandle<void *>();
    eventPoolCloseIpcHandleArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.EventPool.pfnCloseIpcHandle =
        [](ze_event_pool_handle_t hEventPool) -> ze_result_t {
        EXPECT_EQ(eventPoolCloseIpcHandleArgs.hEventPool1, hEventPool);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.EventPool.pfnCloseIpcHandleCb =
        [](ze_event_pool_close_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolCloseIpcHandleArgs.hEventPool0, *params->phEventPool);
        *params->phEventPool = eventPoolCloseIpcHandleArgs.hEventPool1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventPoolCloseIpcHandleArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.EventPool.pfnCloseIpcHandleCb =
        [](ze_event_pool_close_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolCloseIpcHandleArgs.hEventPool1, *params->phEventPool);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventPoolCloseIpcHandleArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.EventPool.pfnCloseIpcHandleCb =
        [](ze_event_pool_close_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolCloseIpcHandleArgs.hEventPool1, *params->phEventPool);
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
        [](ze_event_pool_close_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolCloseIpcHandleArgs.hEventPool1, *params->phEventPool);
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
        [](ze_event_pool_close_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(eventPoolCloseIpcHandleArgs.hEventPool1, *params->phEventPool);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = eventPoolCloseIpcHandleArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.EventPool.pfnCloseIpcHandleCb =
        [](ze_event_pool_close_ipc_handle_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(eventPoolCloseIpcHandleArgs.hEventPool1, *params->phEventPool);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, eventPoolCloseIpcHandleArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeEventPoolCloseIpcHandleTracing(eventPoolCloseIpcHandleArgs.hEventPool0);
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
} commandListAppendSignalEventArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingCommandListAppendSignalEventTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    commandListAppendSignalEventArgs.hCommandList0 = generateRandomHandle<ze_command_list_handle_t>();
    commandListAppendSignalEventArgs.hEvent0 = generateRandomHandle<ze_event_handle_t>();

    // initialize replacement argument set
    commandListAppendSignalEventArgs.hCommandList1 = generateRandomHandle<ze_command_list_handle_t>();
    commandListAppendSignalEventArgs.hEvent1 = generateRandomHandle<ze_event_handle_t>();

    // initialize user instance data
    commandListAppendSignalEventArgs.instanceData0 = generateRandomHandle<void *>();
    commandListAppendSignalEventArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.CommandList.pfnAppendSignalEvent =
        [](ze_command_list_handle_t hCommandList, ze_event_handle_t hEvent) -> ze_result_t {
        EXPECT_EQ(commandListAppendSignalEventArgs.hCommandList1, hCommandList);
        EXPECT_EQ(commandListAppendSignalEventArgs.hEvent1, hEvent);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.CommandList.pfnAppendSignalEventCb =
        [](ze_command_list_append_signal_event_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(commandListAppendSignalEventArgs.hCommandList0, *params->phCommandList);
        EXPECT_EQ(commandListAppendSignalEventArgs.hEvent0, *params->phEvent);
        *params->phCommandList = commandListAppendSignalEventArgs.hCommandList1;
        *params->phEvent = commandListAppendSignalEventArgs.hEvent1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = commandListAppendSignalEventArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.CommandList.pfnAppendSignalEventCb =
        [](ze_command_list_append_signal_event_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(commandListAppendSignalEventArgs.hCommandList1, *params->phCommandList);
        EXPECT_EQ(commandListAppendSignalEventArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, commandListAppendSignalEventArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.CommandList.pfnAppendSignalEventCb =
        [](ze_command_list_append_signal_event_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(commandListAppendSignalEventArgs.hCommandList1, *params->phCommandList);
        EXPECT_EQ(commandListAppendSignalEventArgs.hEvent1, *params->phEvent);
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
        [](ze_command_list_append_signal_event_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(commandListAppendSignalEventArgs.hCommandList1, *params->phCommandList);
        EXPECT_EQ(commandListAppendSignalEventArgs.hEvent1, *params->phEvent);
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
        [](ze_command_list_append_signal_event_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(commandListAppendSignalEventArgs.hCommandList1, *params->phCommandList);
        EXPECT_EQ(commandListAppendSignalEventArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = commandListAppendSignalEventArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    epilogCbs3.CommandList.pfnAppendSignalEventCb =
        [](ze_command_list_append_signal_event_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(commandListAppendSignalEventArgs.hCommandList1, *params->phCommandList);
        EXPECT_EQ(commandListAppendSignalEventArgs.hEvent1, *params->phEvent);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, commandListAppendSignalEventArgs.instanceData0);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendSignalEventTracing(commandListAppendSignalEventArgs.hCommandList0, commandListAppendSignalEventArgs.hEvent0);
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
} commandListAppendWaitOnEventsArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingCommandListAppendWaitOnEventsTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    commandListAppendWaitOnEventsArgs.hCommandList0 = generateRandomHandle<ze_command_list_handle_t>();
    for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_0; i++) {
        commandListAppendWaitOnEventsArgs.hEvents0[i] = generateRandomHandle<ze_event_handle_t>();
    }

    // initialize replacement argument set
    commandListAppendWaitOnEventsArgs.hCommandList1 = generateRandomHandle<ze_command_list_handle_t>();
    for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
        commandListAppendWaitOnEventsArgs.hEvents1[i] = generateRandomHandle<ze_event_handle_t>();
    }

    // initialize user instance data
    commandListAppendWaitOnEventsArgs.instanceData0 = generateRandomHandle<void *>();
    commandListAppendWaitOnEventsArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.CommandList.pfnAppendWaitOnEvents =
        [](ze_command_list_handle_t hCommandList, uint32_t numEvents, ze_event_handle_t *phEvents) -> ze_result_t {
        EXPECT_EQ(commandListAppendWaitOnEventsArgs.hCommandList1, hCommandList);
        EXPECT_EQ(numEvents, (uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1);
        for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
            EXPECT_EQ(commandListAppendWaitOnEventsArgs.hEvents1[i], phEvents[i]);
        }
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.CommandList.pfnAppendWaitOnEventsCb =
        [](ze_command_list_append_wait_on_events_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(commandListAppendWaitOnEventsArgs.hCommandList0, *params->phCommandList);
        EXPECT_EQ((uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_0, *params->pnumEvents);
        for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_0; i++) {
            EXPECT_EQ(commandListAppendWaitOnEventsArgs.hEvents0[i], (*(params->pphEvents))[i]);
        }
        *params->phCommandList = commandListAppendWaitOnEventsArgs.hCommandList1;
        *params->pnumEvents = NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1;
        *params->pphEvents = commandListAppendWaitOnEventsArgs.hEvents1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = commandListAppendWaitOnEventsArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.CommandList.pfnAppendWaitOnEventsCb =
        [](ze_command_list_append_wait_on_events_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(commandListAppendWaitOnEventsArgs.hCommandList1, *params->phCommandList);
        EXPECT_EQ((uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1, *params->pnumEvents);
        for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
            EXPECT_EQ(commandListAppendWaitOnEventsArgs.hEvents1[i], (*(params->pphEvents))[i]);
        }
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, commandListAppendWaitOnEventsArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.CommandList.pfnAppendWaitOnEventsCb =
        [](ze_command_list_append_wait_on_events_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(commandListAppendWaitOnEventsArgs.hCommandList1, *params->phCommandList);
        EXPECT_EQ((uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1, *params->pnumEvents);
        for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
            EXPECT_EQ(commandListAppendWaitOnEventsArgs.hEvents1[i], (*(params->pphEvents))[i]);
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
        [](ze_command_list_append_wait_on_events_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(commandListAppendWaitOnEventsArgs.hCommandList1, *params->phCommandList);
        EXPECT_EQ((uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1, *params->pnumEvents);
        for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
            EXPECT_EQ(commandListAppendWaitOnEventsArgs.hEvents1[i], (*(params->pphEvents))[i]);
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
        [](ze_command_list_append_wait_on_events_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(commandListAppendWaitOnEventsArgs.hCommandList1, *params->phCommandList);
        EXPECT_EQ((uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1, *params->pnumEvents);
        for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
            EXPECT_EQ(commandListAppendWaitOnEventsArgs.hEvents1[i], (*(params->pphEvents))[i]);
        }
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = commandListAppendWaitOnEventsArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    epilogCbs3.CommandList.pfnAppendWaitOnEventsCb =
        [](ze_command_list_append_wait_on_events_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(commandListAppendWaitOnEventsArgs.hCommandList1, *params->phCommandList);
        EXPECT_EQ((uint32_t)NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1, *params->pnumEvents);
        for (int i = 0; i < NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_1; i++) {
            EXPECT_EQ(commandListAppendWaitOnEventsArgs.hEvents1[i], (*(params->pphEvents))[i]);
        }
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, commandListAppendWaitOnEventsArgs.instanceData0);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendWaitOnEventsTracing(commandListAppendWaitOnEventsArgs.hCommandList0,
                                                    NUM_COMMAND_LIST_APPEND_WAIT_ON_EVENTS_0,
                                                    commandListAppendWaitOnEventsArgs.hEvents0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

} // namespace ult
} // namespace L0
