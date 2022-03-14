/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(zeAPITracingRuntimeTests, WhenCallingImageGetPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Image.pfnGetProperties =
        [](ze_device_handle_t hDevice, const ze_image_desc_t *desc, ze_image_properties_t *pImageProperties) { return ZE_RESULT_SUCCESS; };
    const ze_image_desc_t desc = {};
    ze_image_properties_t pImageProperties = {};

    prologCbs.Image.pfnGetPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Image.pfnGetPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeImageGetProperties_Tracing(nullptr, &desc, &pImageProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingImageCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Image.pfnCreate =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, const ze_image_desc_t *desc, ze_image_handle_t *phImage) { return ZE_RESULT_SUCCESS; };
    const ze_image_desc_t desc = {};
    ze_image_handle_t phImage = {};

    prologCbs.Image.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.Image.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeImageCreate_Tracing(nullptr, nullptr, &desc, &phImage);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingImageDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Image.pfnDestroy =
        [](ze_image_handle_t hImage) { return ZE_RESULT_SUCCESS; };
    prologCbs.Image.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.Image.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeImageDestroy_Tracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

// More complex tracing test.

struct {
    ze_device_handle_t hDevice0;
    ze_image_desc_t desc0;
    ze_image_properties_t ImageProperties0;
    ze_device_handle_t hDevice1;
    ze_image_desc_t desc1;
    ze_image_properties_t ImageProperties1;
    void *instanceData0;
    void *instanceData3;
} ImageGetProperties_args;

TEST_F(zeAPITracingRuntimeMultipleArgumentsTests, WhenCallingImageGetPropertiesTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    ImageGetProperties_args.hDevice0 = generateRandomHandle<ze_device_handle_t>();

    // initialize replacement argument set
    ImageGetProperties_args.hDevice1 = generateRandomHandle<ze_device_handle_t>();

    // initialize user instance data
    ImageGetProperties_args.instanceData0 = generateRandomHandle<void *>();
    ImageGetProperties_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Image.pfnGetProperties =
        [](ze_device_handle_t hDevice, const ze_image_desc_t *desc, ze_image_properties_t *pImageProperties) {
            EXPECT_EQ(ImageGetProperties_args.hDevice1, hDevice);
            EXPECT_EQ(&ImageGetProperties_args.desc1, desc);
            EXPECT_EQ(&ImageGetProperties_args.ImageProperties1, pImageProperties);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Image.pfnGetPropertiesCb =
        [](ze_image_get_properties_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(ImageGetProperties_args.hDevice0, *params->phDevice);
            EXPECT_EQ(&ImageGetProperties_args.desc0, *params->pdesc);
            EXPECT_EQ(&ImageGetProperties_args.ImageProperties0, *params->ppImageProperties);
            *params->phDevice = ImageGetProperties_args.hDevice1;
            *params->pdesc = &ImageGetProperties_args.desc1;
            *params->ppImageProperties = &ImageGetProperties_args.ImageProperties1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = ImageGetProperties_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Image.pfnGetPropertiesCb =
        [](ze_image_get_properties_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(ImageGetProperties_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&ImageGetProperties_args.desc1, *params->pdesc);
            EXPECT_EQ(&ImageGetProperties_args.ImageProperties1, *params->ppImageProperties);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, ImageGetProperties_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Image.pfnGetPropertiesCb =
        [](ze_image_get_properties_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(ImageGetProperties_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&ImageGetProperties_args.desc1, *params->pdesc);
            EXPECT_EQ(&ImageGetProperties_args.ImageProperties1, *params->ppImageProperties);
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
        [](ze_image_get_properties_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(ImageGetProperties_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&ImageGetProperties_args.desc1, *params->pdesc);
            EXPECT_EQ(&ImageGetProperties_args.ImageProperties1, *params->ppImageProperties);
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
        [](ze_image_get_properties_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(ImageGetProperties_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&ImageGetProperties_args.desc1, *params->pdesc);
            EXPECT_EQ(&ImageGetProperties_args.ImageProperties1, *params->ppImageProperties);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = ImageGetProperties_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Image.pfnGetPropertiesCb =
        [](ze_image_get_properties_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(ImageGetProperties_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&ImageGetProperties_args.desc1, *params->pdesc);
            EXPECT_EQ(&ImageGetProperties_args.ImageProperties1, *params->ppImageProperties);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, ImageGetProperties_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeImageGetProperties_Tracing(ImageGetProperties_args.hDevice0, &ImageGetProperties_args.desc0, &ImageGetProperties_args.ImageProperties0);
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
} ImageCreate_args;

TEST_F(zeAPITracingRuntimeMultipleArgumentsTests, WhenCallingImageCreateTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    ImageCreate_args.hContext0 = generateRandomHandle<ze_context_handle_t>();
    ImageCreate_args.hDevice0 = generateRandomHandle<ze_device_handle_t>();
    ImageCreate_args.hImage0 = generateRandomHandle<ze_image_handle_t>();

    // initialize replacement argument set
    ImageCreate_args.hContext1 = generateRandomHandle<ze_context_handle_t>();
    ImageCreate_args.hDevice1 = generateRandomHandle<ze_device_handle_t>();
    ImageCreate_args.hImage1 = generateRandomHandle<ze_image_handle_t>();

    // initialize user instance data
    ImageCreate_args.instanceData0 = generateRandomHandle<void *>();
    ImageCreate_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Image.pfnCreate =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, const ze_image_desc_t *desc, ze_image_handle_t *phImage) {
            EXPECT_EQ(ImageCreate_args.hContext1, hContext);
            EXPECT_EQ(ImageCreate_args.hDevice1, hDevice);
            EXPECT_EQ(&ImageCreate_args.desc1, desc);
            EXPECT_EQ(&ImageCreate_args.hImage1, phImage);
            EXPECT_EQ(ImageCreate_args.hImage1, *phImage);
            ImageCreate_args.hImageAPI = generateRandomHandle<ze_image_handle_t>();
            *phImage = ImageCreate_args.hImageAPI;
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Image.pfnCreateCb =
        [](ze_image_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            ASSERT_NE(nullptr, params);
            ASSERT_NE(nullptr, params->phContext);
            ASSERT_NE(nullptr, params->phDevice);
            ASSERT_NE(nullptr, *params->phContext);
            ASSERT_NE(nullptr, *params->phDevice);
            EXPECT_EQ(ImageCreate_args.hContext0, *params->phContext);
            EXPECT_EQ(ImageCreate_args.hDevice0, *params->phDevice);
            EXPECT_EQ(&ImageCreate_args.desc0, *params->pdesc);

            ze_image_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphImage;

            ze_image_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&ImageCreate_args.hImage0, pHandle);

            ze_image_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(ImageCreate_args.hImage0, handle);

            *params->phContext = ImageCreate_args.hContext1;
            *params->phDevice = ImageCreate_args.hDevice1;
            *params->pdesc = &ImageCreate_args.desc1;
            *params->pphImage = &ImageCreate_args.hImage1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = ImageCreate_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Image.pfnCreateCb =
        [](ze_image_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            ASSERT_NE(nullptr, params);
            ASSERT_NE(nullptr, params->phContext);
            ASSERT_NE(nullptr, params->phDevice);
            ASSERT_NE(nullptr, *params->phContext);
            ASSERT_NE(nullptr, *params->phDevice);
            EXPECT_EQ(ImageCreate_args.hContext1, *params->phContext);
            EXPECT_EQ(ImageCreate_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&ImageCreate_args.desc1, *params->pdesc);

            ze_image_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphImage;

            ze_image_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&ImageCreate_args.hImage1, pHandle);

            ze_image_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(ImageCreate_args.hImage1, handle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, ImageCreate_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Image.pfnCreateCb =
        [](ze_image_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(ImageCreate_args.hContext1, *params->phContext);
            EXPECT_EQ(ImageCreate_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&ImageCreate_args.desc1, *params->pdesc);

            ze_image_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphImage;

            ze_image_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&ImageCreate_args.hImage1, pHandle);

            ze_image_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(ImageCreate_args.hImage1, handle);

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
        [](ze_image_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(ImageCreate_args.hContext1, *params->phContext);
            EXPECT_EQ(ImageCreate_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&ImageCreate_args.desc1, *params->pdesc);

            ze_image_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphImage;

            ze_image_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&ImageCreate_args.hImage1, pHandle);

            ze_image_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(ImageCreate_args.hImage1, handle);

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
        [](ze_image_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(ImageCreate_args.hContext1, *params->phContext);
            EXPECT_EQ(ImageCreate_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&ImageCreate_args.desc1, *params->pdesc);

            ze_image_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphImage;

            ze_image_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&ImageCreate_args.hImage1, pHandle);

            ze_image_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(ImageCreate_args.hImage1, handle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = ImageCreate_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Image.pfnCreateCb =
        [](ze_image_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(ImageCreate_args.hContext1, *params->phContext);
            EXPECT_EQ(ImageCreate_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&ImageCreate_args.desc1, *params->pdesc);

            ze_image_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphImage;

            ze_image_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&ImageCreate_args.hImage1, pHandle);

            ze_image_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(ImageCreate_args.hImage1, handle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, ImageCreate_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeImageCreate_Tracing(ImageCreate_args.hContext0, ImageCreate_args.hDevice0, &ImageCreate_args.desc0, &ImageCreate_args.hImage0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_image_handle_t hImage0;
    ze_image_handle_t hImage1;
    void *instanceData0;
    void *instanceData3;
} ImageDestroy_args;

TEST_F(zeAPITracingRuntimeMultipleArgumentsTests, WhenCallingImageDestroyTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    ImageDestroy_args.hImage0 = generateRandomHandle<ze_image_handle_t>();

    // initialize replacement argument set
    ImageDestroy_args.hImage1 = generateRandomHandle<ze_image_handle_t>();

    // initialize user instance data
    ImageDestroy_args.instanceData0 = generateRandomHandle<void *>();
    ImageDestroy_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Image.pfnDestroy =
        [](ze_image_handle_t hImage) {
            EXPECT_EQ(ImageDestroy_args.hImage1, hImage);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Image.pfnDestroyCb =
        [](ze_image_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(ImageDestroy_args.hImage0, *params->phImage);
            *params->phImage = ImageDestroy_args.hImage1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = ImageDestroy_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Image.pfnDestroyCb =
        [](ze_image_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(ImageDestroy_args.hImage1, *params->phImage);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, ImageDestroy_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Image.pfnDestroyCb =
        [](ze_image_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(ImageDestroy_args.hImage1, *params->phImage);
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
        [](ze_image_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(ImageDestroy_args.hImage1, *params->phImage);
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
        [](ze_image_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(ImageDestroy_args.hImage1, *params->phImage);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = ImageDestroy_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Image.pfnDestroyCb =
        [](ze_image_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(ImageDestroy_args.hImage1, *params->phImage);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, ImageDestroy_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeImageDestroy_Tracing(ImageDestroy_args.hImage0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

} // namespace ult
} // namespace L0
