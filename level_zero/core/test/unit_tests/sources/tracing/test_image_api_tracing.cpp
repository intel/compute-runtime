/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingImageGetPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Image.pfnGetProperties =
        [](ze_device_handle_t hDevice, const ze_image_desc_t *desc, ze_image_properties_t *pImageProperties) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    const ze_image_desc_t desc = {};
    ze_image_properties_t pImageProperties = {};

    prologCbs.Image.pfnGetPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Image.pfnGetPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeImageGetPropertiesTracing(nullptr, &desc, &pImageProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingImageCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Image.pfnCreate =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, const ze_image_desc_t *desc, ze_image_handle_t *phImage) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    const ze_image_desc_t desc = {};
    ze_image_handle_t phImage = {};

    prologCbs.Image.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.Image.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeImageCreateTracing(nullptr, nullptr, &desc, &phImage);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingImageDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Image.pfnDestroy =
        [](ze_image_handle_t hImage) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    prologCbs.Image.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.Image.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeImageDestroyTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

// More complex tracing test.

struct {
    ze_device_handle_t hDevice0;
    ze_image_desc_t desc0;
    ze_image_properties_t imageProperties0;
    ze_device_handle_t hDevice1;
    ze_image_desc_t desc1;
    ze_image_properties_t imageProperties1;
    void *instanceData0;
    void *instanceData3;
} imageGetPropertiesArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingImageGetPropertiesTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    imageGetPropertiesArgs.hDevice0 = generateRandomHandle<ze_device_handle_t>();

    // initialize replacement argument set
    imageGetPropertiesArgs.hDevice1 = generateRandomHandle<ze_device_handle_t>();

    // initialize user instance data
    imageGetPropertiesArgs.instanceData0 = generateRandomHandle<void *>();
    imageGetPropertiesArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Image.pfnGetProperties =
        [](ze_device_handle_t hDevice, const ze_image_desc_t *desc, ze_image_properties_t *pImageProperties) -> ze_result_t {
        EXPECT_EQ(imageGetPropertiesArgs.hDevice1, hDevice);
        EXPECT_EQ(&imageGetPropertiesArgs.desc1, desc);
        EXPECT_EQ(&imageGetPropertiesArgs.imageProperties1, pImageProperties);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Image.pfnGetPropertiesCb =
        [](ze_image_get_properties_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(imageGetPropertiesArgs.hDevice0, *params->phDevice);
        EXPECT_EQ(&imageGetPropertiesArgs.desc0, *params->pdesc);
        EXPECT_EQ(&imageGetPropertiesArgs.imageProperties0, *params->ppImageProperties);
        *params->phDevice = imageGetPropertiesArgs.hDevice1;
        *params->pdesc = &imageGetPropertiesArgs.desc1;
        *params->ppImageProperties = &imageGetPropertiesArgs.imageProperties1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = imageGetPropertiesArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Image.pfnGetPropertiesCb =
        [](ze_image_get_properties_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(imageGetPropertiesArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&imageGetPropertiesArgs.desc1, *params->pdesc);
        EXPECT_EQ(&imageGetPropertiesArgs.imageProperties1, *params->ppImageProperties);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, imageGetPropertiesArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Image.pfnGetPropertiesCb =
        [](ze_image_get_properties_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(imageGetPropertiesArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&imageGetPropertiesArgs.desc1, *params->pdesc);
        EXPECT_EQ(&imageGetPropertiesArgs.imageProperties1, *params->ppImageProperties);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 11);
        *val += 11;
    };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Image.pfnGetPropertiesCb =
        [](ze_image_get_properties_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(imageGetPropertiesArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&imageGetPropertiesArgs.desc1, *params->pdesc);
        EXPECT_EQ(&imageGetPropertiesArgs.imageProperties1, *params->ppImageProperties);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 21);
        *val += 21;
    };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Image.pfnGetPropertiesCb =
        [](ze_image_get_properties_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(imageGetPropertiesArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&imageGetPropertiesArgs.desc1, *params->pdesc);
        EXPECT_EQ(&imageGetPropertiesArgs.imageProperties1, *params->ppImageProperties);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = imageGetPropertiesArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Image.pfnGetPropertiesCb =
        [](ze_image_get_properties_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(imageGetPropertiesArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&imageGetPropertiesArgs.desc1, *params->pdesc);
        EXPECT_EQ(&imageGetPropertiesArgs.imageProperties1, *params->ppImageProperties);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, imageGetPropertiesArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeImageGetPropertiesTracing(imageGetPropertiesArgs.hDevice0, &imageGetPropertiesArgs.desc0, &imageGetPropertiesArgs.imageProperties0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_context_handle_t hContext0;
    ze_device_handle_t hDevice0;
    ze_image_desc_t desc0;
    ze_image_handle_t hImage0;
    ze_context_handle_t hContext1;
    ze_device_handle_t hDevice1;
    ze_image_desc_t desc1;
    ze_image_handle_t hImage1;
    ze_image_handle_t hImageAPI;
    void *instanceData0;
    void *instanceData3;
} imageCreateArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingImageCreateTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    imageCreateArgs.hContext0 = generateRandomHandle<ze_context_handle_t>();
    imageCreateArgs.hDevice0 = generateRandomHandle<ze_device_handle_t>();
    imageCreateArgs.hImage0 = generateRandomHandle<ze_image_handle_t>();

    // initialize replacement argument set
    imageCreateArgs.hContext1 = generateRandomHandle<ze_context_handle_t>();
    imageCreateArgs.hDevice1 = generateRandomHandle<ze_device_handle_t>();
    imageCreateArgs.hImage1 = generateRandomHandle<ze_image_handle_t>();

    // initialize user instance data
    imageCreateArgs.instanceData0 = generateRandomHandle<void *>();
    imageCreateArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Image.pfnCreate =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, const ze_image_desc_t *desc, ze_image_handle_t *phImage) -> ze_result_t {
        EXPECT_EQ(imageCreateArgs.hContext1, hContext);
        EXPECT_EQ(imageCreateArgs.hDevice1, hDevice);
        EXPECT_EQ(&imageCreateArgs.desc1, desc);
        EXPECT_EQ(&imageCreateArgs.hImage1, phImage);
        EXPECT_EQ(imageCreateArgs.hImage1, *phImage);
        imageCreateArgs.hImageAPI = generateRandomHandle<ze_image_handle_t>();
        *phImage = imageCreateArgs.hImageAPI;
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Image.pfnCreateCb =
        [](ze_image_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        ASSERT_NE(nullptr, params);
        ASSERT_NE(nullptr, params->phContext);
        ASSERT_NE(nullptr, params->phDevice);
        ASSERT_NE(nullptr, *params->phContext);
        ASSERT_NE(nullptr, *params->phDevice);
        EXPECT_EQ(imageCreateArgs.hContext0, *params->phContext);
        EXPECT_EQ(imageCreateArgs.hDevice0, *params->phDevice);
        EXPECT_EQ(&imageCreateArgs.desc0, *params->pdesc);

        ze_image_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphImage;

        ze_image_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;
        EXPECT_EQ(&imageCreateArgs.hImage0, pHandle);

        ze_image_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(imageCreateArgs.hImage0, handle);

        *params->phContext = imageCreateArgs.hContext1;
        *params->phDevice = imageCreateArgs.hDevice1;
        *params->pdesc = &imageCreateArgs.desc1;
        *params->pphImage = &imageCreateArgs.hImage1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = imageCreateArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Image.pfnCreateCb =
        [](ze_image_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        ASSERT_NE(nullptr, params);
        ASSERT_NE(nullptr, params->phContext);
        ASSERT_NE(nullptr, params->phDevice);
        ASSERT_NE(nullptr, *params->phContext);
        ASSERT_NE(nullptr, *params->phDevice);
        EXPECT_EQ(imageCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(imageCreateArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&imageCreateArgs.desc1, *params->pdesc);

        ze_image_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphImage;

        ze_image_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;
        EXPECT_EQ(&imageCreateArgs.hImage1, pHandle);

        ze_image_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(imageCreateArgs.hImage1, handle);

        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, imageCreateArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Image.pfnCreateCb =
        [](ze_image_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(imageCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(imageCreateArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&imageCreateArgs.desc1, *params->pdesc);

        ze_image_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphImage;

        ze_image_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;
        EXPECT_EQ(&imageCreateArgs.hImage1, pHandle);

        ze_image_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(imageCreateArgs.hImage1, handle);

        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 11);
        *val += 11;
    };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Image.pfnCreateCb =
        [](ze_image_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(imageCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(imageCreateArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&imageCreateArgs.desc1, *params->pdesc);

        ze_image_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphImage;

        ze_image_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;
        EXPECT_EQ(&imageCreateArgs.hImage1, pHandle);

        ze_image_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(imageCreateArgs.hImage1, handle);

        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 21);
        *val += 21;
    };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Image.pfnCreateCb =
        [](ze_image_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(imageCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(imageCreateArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&imageCreateArgs.desc1, *params->pdesc);

        ze_image_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphImage;

        ze_image_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;
        EXPECT_EQ(&imageCreateArgs.hImage1, pHandle);

        ze_image_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(imageCreateArgs.hImage1, handle);

        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = imageCreateArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Image.pfnCreateCb =
        [](ze_image_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(imageCreateArgs.hContext1, *params->phContext);
        EXPECT_EQ(imageCreateArgs.hDevice1, *params->phDevice);
        EXPECT_EQ(&imageCreateArgs.desc1, *params->pdesc);

        ze_image_handle_t **ppHandle;
        ASSERT_NE(nullptr, params);
        ppHandle = params->pphImage;

        ze_image_handle_t *pHandle;
        ASSERT_NE(nullptr, ppHandle);
        pHandle = *ppHandle;
        EXPECT_EQ(&imageCreateArgs.hImage1, pHandle);

        ze_image_handle_t handle;
        ASSERT_NE(nullptr, pHandle);
        handle = *pHandle;
        EXPECT_EQ(imageCreateArgs.hImage1, handle);

        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, imageCreateArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeImageCreateTracing(imageCreateArgs.hContext0, imageCreateArgs.hDevice0, &imageCreateArgs.desc0, &imageCreateArgs.hImage0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_image_handle_t hImage0;
    ze_image_handle_t hImage1;
    void *instanceData0;
    void *instanceData3;
} imageDestroyArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingImageDestroyTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    imageDestroyArgs.hImage0 = generateRandomHandle<ze_image_handle_t>();

    // initialize replacement argument set
    imageDestroyArgs.hImage1 = generateRandomHandle<ze_image_handle_t>();

    // initialize user instance data
    imageDestroyArgs.instanceData0 = generateRandomHandle<void *>();
    imageDestroyArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Image.pfnDestroy =
        [](ze_image_handle_t hImage) -> ze_result_t {
        EXPECT_EQ(imageDestroyArgs.hImage1, hImage);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Image.pfnDestroyCb =
        [](ze_image_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(imageDestroyArgs.hImage0, *params->phImage);
        *params->phImage = imageDestroyArgs.hImage1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = imageDestroyArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Image.pfnDestroyCb =
        [](ze_image_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(imageDestroyArgs.hImage1, *params->phImage);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, imageDestroyArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Image.pfnDestroyCb =
        [](ze_image_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(imageDestroyArgs.hImage1, *params->phImage);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 11);
        *val += 11;
    };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Image.pfnDestroyCb =
        [](ze_image_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(imageDestroyArgs.hImage1, *params->phImage);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 21);
        *val += 21;
    };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Image.pfnDestroyCb =
        [](ze_image_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(imageDestroyArgs.hImage1, *params->phImage);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = imageDestroyArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Image.pfnDestroyCb =
        [](ze_image_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(imageDestroyArgs.hImage1, *params->phImage);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, imageDestroyArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeImageDestroyTracing(imageDestroyArgs.hImage0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

} // namespace ult
} // namespace L0
