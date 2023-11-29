/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingSamplerCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Sampler.pfnCreate = [](ze_context_handle_t hContext, ze_device_handle_t hDevice, const ze_sampler_desc_t *pDesc, ze_sampler_handle_t *phSampler) -> ze_result_t { return ZE_RESULT_SUCCESS; };

    prologCbs.Sampler.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.Sampler.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeSamplerCreateTracing(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingSamplerDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Sampler.pfnDestroy = [](ze_sampler_handle_t hSampler) -> ze_result_t { return ZE_RESULT_SUCCESS; };

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
    ze_sampler_desc_t desc0;
    ze_sampler_handle_t hSampler0;
    ze_context_handle_t hContext1;
    ze_device_handle_t hDevice1;
    ze_sampler_desc_t desc1;
    ze_sampler_handle_t hSampler1;
    ze_sampler_handle_t hSamplerAPI;
    void *instanceData0;
    void *instanceData3;
} samplerCreateArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests,
       WhenCallingSamplerCreateTracingWrapperWithTwoSetsOfPrologEpilogsCheckArgumentsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    samplerCreateArgs.hContext0 = generateRandomHandle<ze_context_handle_t>();
    samplerCreateArgs.hDevice0 = generateRandomHandle<ze_device_handle_t>();
    samplerCreateArgs.hSampler0 = generateRandomHandle<ze_sampler_handle_t>();

    // initialize replacement argument set
    samplerCreateArgs.hContext1 = generateRandomHandle<ze_context_handle_t>();
    samplerCreateArgs.hDevice1 = generateRandomHandle<ze_device_handle_t>();
    samplerCreateArgs.hSampler1 = generateRandomHandle<ze_sampler_handle_t>();

    // initialize user instance data
    samplerCreateArgs.instanceData0 = generateRandomHandle<void *>();
    samplerCreateArgs.instanceData3 = generateRandomHandle<void *>();

    // Arguments are expected to be passed in by the first prolog callback
    driverDdiTable.coreDdiTable.Sampler.pfnCreate =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, const ze_sampler_desc_t *pDesc, ze_sampler_handle_t *phSampler) -> ze_result_t {
        EXPECT_EQ(samplerCreateArgs.hContext1, hContext);
        EXPECT_EQ(samplerCreateArgs.hDevice1, hDevice);
        EXPECT_EQ(&samplerCreateArgs.desc1, pDesc);
        EXPECT_EQ(&samplerCreateArgs.hSampler1, phSampler);
        EXPECT_EQ(samplerCreateArgs.hSampler1, *phSampler);
        samplerCreateArgs.hSamplerAPI = generateRandomHandle<ze_sampler_handle_t>();
        *phSampler = samplerCreateArgs.hSamplerAPI;
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Sampler.pfnCreateCb =
        [](ze_sampler_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(samplerCreateArgs.hContext0, *params->phContext);
        EXPECT_EQ(samplerCreateArgs.hDevice0, *params->phDevice);
        EXPECT_EQ(&samplerCreateArgs.desc0, *params->pdesc);
        EXPECT_EQ(&samplerCreateArgs.hSampler0, *params->pphSampler);

        ze_sampler_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphSampler;

        ze_sampler_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_sampler_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(samplerCreateArgs.hSampler0, handle);
        *params->phContext = samplerCreateArgs.hContext1;
        *params->phDevice = samplerCreateArgs.hDevice1;
        *params->pdesc = &samplerCreateArgs.desc1;
        *params->pphSampler = &samplerCreateArgs.hSampler1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = samplerCreateArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Sampler.pfnCreateCb =
        [](ze_sampler_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(samplerCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(samplerCreateArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&samplerCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&samplerCreateArgs.hSampler1, *params->pphSampler);

        ze_sampler_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphSampler;

        ze_sampler_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_sampler_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(samplerCreateArgs.hSampler1, handle);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, samplerCreateArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Sampler.pfnCreateCb =
        [](ze_sampler_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(samplerCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(samplerCreateArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&samplerCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&samplerCreateArgs.hSampler1, *params->pphSampler);

        ze_sampler_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphSampler;

        ze_sampler_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_sampler_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(samplerCreateArgs.hSampler1, handle);
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
        [](ze_sampler_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(samplerCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(samplerCreateArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&samplerCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&samplerCreateArgs.hSampler1, *params->pphSampler);

        ze_sampler_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphSampler;

        ze_sampler_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_sampler_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(samplerCreateArgs.hSampler1, handle);
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
        [](ze_sampler_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(samplerCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(samplerCreateArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&samplerCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&samplerCreateArgs.hSampler1, *params->pphSampler);

        ze_sampler_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphSampler;

        ze_sampler_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_sampler_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(samplerCreateArgs.hSampler1, handle);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = samplerCreateArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Sampler.pfnCreateCb =
        [](ze_sampler_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(samplerCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(samplerCreateArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&samplerCreateArgs.desc1, *params->pdesc);
        EXPECT_EQ(&samplerCreateArgs.hSampler1, *params->pphSampler);

        ze_sampler_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphSampler;

        ze_sampler_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;

        ze_sampler_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;

        EXPECT_EQ(samplerCreateArgs.hSampler1, handle);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, samplerCreateArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeSamplerCreateTracing(samplerCreateArgs.hContext0, samplerCreateArgs.hDevice0, &samplerCreateArgs.desc0, &samplerCreateArgs.hSampler0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(samplerCreateArgs.hSampler1, samplerCreateArgs.hSamplerAPI);
    validateDefaultUserDataFinal();
}

struct {
    ze_sampler_handle_t hSampler0;
    ze_sampler_handle_t hSampler1;
    void *instanceData0;
    void *instanceData3;
} samplerDestroyArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests,
       WhenCallingSamplerDestroyTracingWrapperWithTwoSetsOfPrologEpilogsCheckArgumentsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    samplerDestroyArgs.hSampler0 = generateRandomHandle<ze_sampler_handle_t>();

    // initialize replacement argument set
    samplerDestroyArgs.hSampler1 = generateRandomHandle<ze_sampler_handle_t>();

    // initialize user instance data
    samplerDestroyArgs.instanceData0 = generateRandomHandle<void *>();
    samplerDestroyArgs.instanceData3 = generateRandomHandle<void *>();

    // Arguments are expected to be passed in by the first prolog callback
    driverDdiTable.coreDdiTable.Sampler.pfnDestroy =
        [](ze_sampler_handle_t hSampler) -> ze_result_t {
        EXPECT_EQ(samplerDestroyArgs.hSampler1, hSampler);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Allocate instance data, pass it to corresponding epilog.
    //
    prologCbs0.Sampler.pfnDestroyCb =
        [](ze_sampler_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(samplerDestroyArgs.hSampler0, *params->phSampler);
        *params->phSampler = samplerDestroyArgs.hSampler1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = samplerDestroyArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Sampler.pfnDestroyCb =
        [](ze_sampler_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(samplerDestroyArgs.hSampler1, *params->phSampler);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, samplerDestroyArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Sampler.pfnDestroyCb =
        [](ze_sampler_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(samplerDestroyArgs.hSampler1, *params->phSampler);
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
        [](ze_sampler_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(samplerDestroyArgs.hSampler1, *params->phSampler);
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
        [](ze_sampler_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(samplerDestroyArgs.hSampler1, *params->phSampler);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = samplerDestroyArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Sampler.pfnDestroyCb =
        [](ze_sampler_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(samplerDestroyArgs.hSampler1, *params->phSampler);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, samplerDestroyArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeSamplerDestroyTracing(samplerDestroyArgs.hSampler0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

} // namespace ult
} // namespace L0
