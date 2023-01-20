/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingSamplerCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.Sampler.pfnCreate = [](ze_context_handle_t hContext, ze_device_handle_t hDevice, const ze_sampler_desc_t *pDesc, ze_sampler_handle_t *phSampler) { return ZE_RESULT_SUCCESS; };

    prologCbs.Sampler.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.Sampler.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeSamplerCreateTracing(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingSamplerDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.Sampler.pfnDestroy = [](ze_sampler_handle_t hSampler) { return ZE_RESULT_SUCCESS; };

    prologCbs.Sampler.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.Sampler.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeSamplerDestroyTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

struct {
    ze_context_handle_t hContext0;
    ze_device_handle_t hDevice0;
    ze_sampler_desc_t Desc0;
    ze_sampler_handle_t hSampler0;
    ze_context_handle_t hContext1;
    ze_device_handle_t hDevice1;
    ze_sampler_desc_t Desc1;
    ze_sampler_handle_t hSampler1;
    ze_sampler_handle_t hSamplerAPI;
    void *instanceData0;
    void *instanceData3;
} sampler_create_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests,
       WhenCallingSamplerCreateTracingWrapperWithTwoSetsOfPrologEpilogsCheckArgumentsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    sampler_create_args.hContext0 = generateRandomHandle<ze_context_handle_t>();
    sampler_create_args.hDevice0 = generateRandomHandle<ze_device_handle_t>();
    sampler_create_args.hSampler0 = generateRandomHandle<ze_sampler_handle_t>();

    // initialize replacement argument set
    sampler_create_args.hContext1 = generateRandomHandle<ze_context_handle_t>();
    sampler_create_args.hDevice1 = generateRandomHandle<ze_device_handle_t>();
    sampler_create_args.hSampler1 = generateRandomHandle<ze_sampler_handle_t>();

    // initialize user instance data
    sampler_create_args.instanceData0 = generateRandomHandle<void *>();
    sampler_create_args.instanceData3 = generateRandomHandle<void *>();

    // Arguments are expected to be passed in by the first prolog callback
    driver_ddiTable.core_ddiTable.Sampler.pfnCreate =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, const ze_sampler_desc_t *pDesc, ze_sampler_handle_t *phSampler) {
            EXPECT_EQ(sampler_create_args.hContext1, hContext);
            EXPECT_EQ(sampler_create_args.hDevice1, hDevice);
            EXPECT_EQ(&sampler_create_args.Desc1, pDesc);
            EXPECT_EQ(&sampler_create_args.hSampler1, phSampler);
            EXPECT_EQ(sampler_create_args.hSampler1, *phSampler);
            sampler_create_args.hSamplerAPI = generateRandomHandle<ze_sampler_handle_t>();
            *phSampler = sampler_create_args.hSamplerAPI;
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Sampler.pfnCreateCb =
        [](ze_sampler_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(sampler_create_args.hContext0, *params->phContext);
            EXPECT_EQ(sampler_create_args.hDevice0, *params->phDevice);
            EXPECT_EQ(&sampler_create_args.Desc0, *params->pdesc);
            EXPECT_EQ(&sampler_create_args.hSampler0, *params->pphSampler);

            ze_sampler_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphSampler;

            ze_sampler_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_sampler_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(sampler_create_args.hSampler0, handle);
            *params->phContext = sampler_create_args.hContext1;
            *params->phDevice = sampler_create_args.hDevice1;
            *params->pdesc = &sampler_create_args.Desc1;
            *params->pphSampler = &sampler_create_args.hSampler1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = sampler_create_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Sampler.pfnCreateCb =
        [](ze_sampler_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(sampler_create_args.hContext1, *params->phContext);
            EXPECT_EQ(sampler_create_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&sampler_create_args.Desc1, *params->pdesc);
            EXPECT_EQ(&sampler_create_args.hSampler1, *params->pphSampler);

            ze_sampler_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphSampler;

            ze_sampler_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_sampler_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(sampler_create_args.hSampler1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, sampler_create_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Sampler.pfnCreateCb =
        [](ze_sampler_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(sampler_create_args.hContext1, *params->phContext);
            EXPECT_EQ(sampler_create_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&sampler_create_args.Desc1, *params->pdesc);
            EXPECT_EQ(&sampler_create_args.hSampler1, *params->pphSampler);

            ze_sampler_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphSampler;

            ze_sampler_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_sampler_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(sampler_create_args.hSampler1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Sampler.pfnCreateCb =
        [](ze_sampler_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(sampler_create_args.hContext1, *params->phContext);
            EXPECT_EQ(sampler_create_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&sampler_create_args.Desc1, *params->pdesc);
            EXPECT_EQ(&sampler_create_args.hSampler1, *params->pphSampler);

            ze_sampler_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphSampler;

            ze_sampler_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_sampler_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(sampler_create_args.hSampler1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Sampler.pfnCreateCb =
        [](ze_sampler_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(sampler_create_args.hContext1, *params->phContext);
            EXPECT_EQ(sampler_create_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&sampler_create_args.Desc1, *params->pdesc);
            EXPECT_EQ(&sampler_create_args.hSampler1, *params->pphSampler);

            ze_sampler_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphSampler;

            ze_sampler_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_sampler_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(sampler_create_args.hSampler1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = sampler_create_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Sampler.pfnCreateCb =
        [](ze_sampler_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(sampler_create_args.hContext1, *params->phContext);
            EXPECT_EQ(sampler_create_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&sampler_create_args.Desc1, *params->pdesc);
            EXPECT_EQ(&sampler_create_args.hSampler1, *params->pphSampler);

            ze_sampler_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphSampler;

            ze_sampler_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;

            ze_sampler_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;

            EXPECT_EQ(sampler_create_args.hSampler1, handle);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, sampler_create_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeSamplerCreateTracing(sampler_create_args.hContext0, sampler_create_args.hDevice0, &sampler_create_args.Desc0, &sampler_create_args.hSampler0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(sampler_create_args.hSampler1, sampler_create_args.hSamplerAPI);
    validateDefaultUserDataFinal();
}

struct {
    ze_sampler_handle_t hSampler0;
    ze_sampler_handle_t hSampler1;
    void *instanceData0;
    void *instanceData3;
} sampler_destroy_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests,
       WhenCallingSamplerDestroyTracingWrapperWithTwoSetsOfPrologEpilogsCheckArgumentsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    sampler_destroy_args.hSampler0 = generateRandomHandle<ze_sampler_handle_t>();

    // initialize replacement argument set
    sampler_destroy_args.hSampler1 = generateRandomHandle<ze_sampler_handle_t>();

    // initialize user instance data
    sampler_destroy_args.instanceData0 = generateRandomHandle<void *>();
    sampler_destroy_args.instanceData3 = generateRandomHandle<void *>();

    // Arguments are expected to be passed in by the first prolog callback
    driver_ddiTable.core_ddiTable.Sampler.pfnDestroy =
        [](ze_sampler_handle_t hSampler) {
            EXPECT_EQ(sampler_destroy_args.hSampler1, hSampler);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Allocate instance data, pass it to corresponding epilog.
    //
    prologCbs0.Sampler.pfnDestroyCb =
        [](ze_sampler_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(sampler_destroy_args.hSampler0, *params->phSampler);
            *params->phSampler = sampler_destroy_args.hSampler1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = sampler_destroy_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Sampler.pfnDestroyCb =
        [](ze_sampler_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(sampler_destroy_args.hSampler1, *params->phSampler);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, sampler_destroy_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Sampler.pfnDestroyCb =
        [](ze_sampler_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(sampler_destroy_args.hSampler1, *params->phSampler);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Sampler.pfnDestroyCb =
        [](ze_sampler_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(sampler_destroy_args.hSampler1, *params->phSampler);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Allocate instance data and pass to corresponding epilog
    //
    prologCbs3.Sampler.pfnDestroyCb =
        [](ze_sampler_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(sampler_destroy_args.hSampler1, *params->phSampler);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = sampler_destroy_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Sampler.pfnDestroyCb =
        [](ze_sampler_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(sampler_destroy_args.hSampler1, *params->phSampler);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, sampler_destroy_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeSamplerDestroyTracing(sampler_destroy_args.hSampler0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

} // namespace ult
} // namespace L0
