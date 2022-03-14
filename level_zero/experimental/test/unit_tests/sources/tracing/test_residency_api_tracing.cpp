/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(zeAPITracingRuntimeTests, WhenCallingContextCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Context.pfnCreate = [](ze_driver_handle_t hContext, const ze_context_desc_t *desc, ze_context_handle_t *phContext) { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextCreate_Tracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingContextDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Context.pfnDestroy = [](ze_context_handle_t hContext) { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextDestroy_Tracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingContextGetStatusTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Context.pfnGetStatus = [](ze_context_handle_t hContext) { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnGetStatusCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnGetStatusCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextGetStatus_Tracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingContextSystemBarrierTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Context.pfnSystemBarrier = [](ze_context_handle_t hContext, ze_device_handle_t hDevice) { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnSystemBarrierCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnSystemBarrierCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextSystemBarrier_Tracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingContextMakeMemoryResidentTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Context.pfnMakeMemoryResident = [](ze_context_handle_t hContext, ze_device_handle_t hDevice, void *ptr, size_t size) { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnMakeMemoryResidentCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnMakeMemoryResidentCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextMakeMemoryResident_Tracing(nullptr, nullptr, nullptr, 1024);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingContextEvictMemoryTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Context.pfnEvictMemory = [](ze_context_handle_t hContext, ze_device_handle_t hDevice, void *ptr, size_t size) { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnEvictMemoryCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnEvictMemoryCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextEvictMemory_Tracing(nullptr, nullptr, nullptr, 1024);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingContextMakeImageResidentTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Context.pfnMakeImageResident = [](ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_image_handle_t hImage) { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnMakeImageResidentCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnMakeImageResidentCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextMakeImageResident_Tracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingContextEvictImageTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Context.pfnEvictImage = [](ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_image_handle_t hImage) { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnEvictImageCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnEvictImageCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextEvictImage_Tracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

struct {
    ze_context_handle_t hContext0;
    ze_device_handle_t hDevice0;
    void *ptr0;
    size_t size0;
    ze_context_handle_t hContext1;
    ze_device_handle_t hDevice1;
    void *ptr1;
    size_t size1;
    void *instanceData0;
    void *instanceData3;
} MakeMemoryResident_args;

TEST_F(zeAPITracingRuntimeMultipleArgumentsTests,
       WhenCallingContextMakeMemoryResidentTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    MakeMemoryResident_args.hContext0 = generateRandomHandle<ze_context_handle_t>();
    MakeMemoryResident_args.hDevice0 = generateRandomHandle<ze_device_handle_t>();
    MakeMemoryResident_args.ptr0 = generateRandomHandle<void *>();
    MakeMemoryResident_args.size0 = generateRandomSize<size_t>();

    // initialize replacement argument set
    MakeMemoryResident_args.hContext1 = generateRandomHandle<ze_context_handle_t>();
    MakeMemoryResident_args.hDevice1 = generateRandomHandle<ze_device_handle_t>();
    MakeMemoryResident_args.ptr1 = generateRandomHandle<void *>();
    MakeMemoryResident_args.size1 = generateRandomSize<size_t>();

    // initialize user instance data
    MakeMemoryResident_args.instanceData0 = generateRandomHandle<void *>();
    MakeMemoryResident_args.instanceData3 = generateRandomHandle<void *>();

    // arguments are expeted to be passed in from first prolog callback
    driver_ddiTable.core_ddiTable.Context.pfnMakeMemoryResident =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, void *ptr, size_t size) {
            EXPECT_EQ(hContext, MakeMemoryResident_args.hContext1);
            EXPECT_EQ(hDevice, MakeMemoryResident_args.hDevice1);
            EXPECT_EQ(ptr, MakeMemoryResident_args.ptr1);
            EXPECT_EQ(size, MakeMemoryResident_args.size1);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Allocate instance data, pass it to corresponding epilog.
    //
    prologCbs0.Context.pfnMakeMemoryResidentCb =
        [](ze_context_make_memory_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(*params->phContext, MakeMemoryResident_args.hContext0);
            EXPECT_EQ(*params->phDevice, MakeMemoryResident_args.hDevice0);
            EXPECT_EQ(*params->pptr, MakeMemoryResident_args.ptr0);
            EXPECT_EQ(*params->psize, MakeMemoryResident_args.size0);
            *params->phContext = MakeMemoryResident_args.hContext1;
            *params->phDevice = MakeMemoryResident_args.hDevice1;
            *params->pptr = MakeMemoryResident_args.ptr1;
            *params->psize = MakeMemoryResident_args.size1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = MakeMemoryResident_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Context.pfnMakeMemoryResidentCb =
        [](ze_context_make_memory_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(*params->phContext, MakeMemoryResident_args.hContext1);
            EXPECT_EQ(*params->phDevice, MakeMemoryResident_args.hDevice1);
            EXPECT_EQ(*params->pptr, MakeMemoryResident_args.ptr1);
            EXPECT_EQ(*params->psize, MakeMemoryResident_args.size1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, MakeMemoryResident_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Context.pfnMakeMemoryResidentCb =
        [](ze_context_make_memory_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(*params->phContext, MakeMemoryResident_args.hContext1);
            EXPECT_EQ(*params->phDevice, MakeMemoryResident_args.hDevice1);
            EXPECT_EQ(*params->pptr, MakeMemoryResident_args.ptr1);
            EXPECT_EQ(*params->psize, MakeMemoryResident_args.size1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Context.pfnMakeMemoryResidentCb =
        [](ze_context_make_memory_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(*params->phContext, MakeMemoryResident_args.hContext1);
            EXPECT_EQ(*params->phDevice, MakeMemoryResident_args.hDevice1);
            EXPECT_EQ(*params->pptr, MakeMemoryResident_args.ptr1);
            EXPECT_EQ(*params->psize, MakeMemoryResident_args.size1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Allocate instance data and pass to corresponding epilog
    //
    prologCbs3.Context.pfnMakeMemoryResidentCb =
        [](ze_context_make_memory_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(*params->phContext, MakeMemoryResident_args.hContext1);
            EXPECT_EQ(*params->phDevice, MakeMemoryResident_args.hDevice1);
            EXPECT_EQ(*params->pptr, MakeMemoryResident_args.ptr1);
            EXPECT_EQ(*params->psize, MakeMemoryResident_args.size1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = MakeMemoryResident_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    epilogCbs3.Context.pfnMakeMemoryResidentCb =
        [](ze_context_make_memory_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(*params->phContext, MakeMemoryResident_args.hContext1);
            EXPECT_EQ(*params->phDevice, MakeMemoryResident_args.hDevice1);
            EXPECT_EQ(*params->pptr, MakeMemoryResident_args.ptr1);
            EXPECT_EQ(*params->psize, MakeMemoryResident_args.size1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, MakeMemoryResident_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeContextMakeMemoryResident_Tracing(MakeMemoryResident_args.hContext0, MakeMemoryResident_args.hDevice0, MakeMemoryResident_args.ptr0, MakeMemoryResident_args.size0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_context_handle_t hContext0;
    ze_device_handle_t hDevice0;
    void *ptr0;
    size_t size0;
    ze_context_handle_t hContext1;
    ze_device_handle_t hDevice1;
    void *ptr1;
    size_t size1;
    void *instanceData0;
    void *instanceData3;
} EvictMemory_args;

TEST_F(zeAPITracingRuntimeMultipleArgumentsTests, WhenCallingContextEvictMemoryTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    EvictMemory_args.hContext0 = generateRandomHandle<ze_context_handle_t>();
    EvictMemory_args.hDevice0 = generateRandomHandle<ze_device_handle_t>();
    EvictMemory_args.ptr0 = generateRandomHandle<void *>();
    EvictMemory_args.size0 = generateRandomSize<size_t>();

    // initialize replacement argument set
    EvictMemory_args.hContext1 = generateRandomHandle<ze_context_handle_t>();
    EvictMemory_args.hDevice1 = generateRandomHandle<ze_device_handle_t>();
    EvictMemory_args.ptr1 = generateRandomHandle<void *>();
    EvictMemory_args.size1 = generateRandomSize<size_t>();

    // initialize user instance data
    EvictMemory_args.instanceData0 = generateRandomHandle<void *>();
    EvictMemory_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Context.pfnEvictMemory =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, void *ptr, size_t size) {
            EXPECT_EQ(hContext, EvictMemory_args.hContext1);
            EXPECT_EQ(hDevice, EvictMemory_args.hDevice1);
            EXPECT_EQ(ptr, EvictMemory_args.ptr1);
            EXPECT_EQ(size, EvictMemory_args.size1);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Context.pfnEvictMemoryCb =
        [](ze_context_evict_memory_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(EvictMemory_args.hContext0, *params->phContext);
            EXPECT_EQ(EvictMemory_args.hDevice0, *params->phDevice);
            EXPECT_EQ(EvictMemory_args.ptr0, *params->pptr);
            EXPECT_EQ(EvictMemory_args.size0, *params->psize);

            *params->phContext = EvictMemory_args.hContext1;
            *params->phDevice = EvictMemory_args.hDevice1;
            *params->pptr = EvictMemory_args.ptr1;
            *params->psize = EvictMemory_args.size1;

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = EvictMemory_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Context.pfnEvictMemoryCb =
        [](ze_context_evict_memory_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(*params->phContext, EvictMemory_args.hContext1);
            EXPECT_EQ(*params->phDevice, EvictMemory_args.hDevice1);
            EXPECT_EQ(*params->pptr, EvictMemory_args.ptr1);
            EXPECT_EQ(*params->psize, EvictMemory_args.size1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, EvictMemory_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Context.pfnEvictMemoryCb =
        [](ze_context_evict_memory_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(*params->phContext, EvictMemory_args.hContext1);
            EXPECT_EQ(*params->phDevice, EvictMemory_args.hDevice1);
            EXPECT_EQ(*params->pptr, EvictMemory_args.ptr1);
            EXPECT_EQ(*params->psize, EvictMemory_args.size1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Context.pfnEvictMemoryCb =
        [](ze_context_evict_memory_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(*params->phContext, EvictMemory_args.hContext1);
            EXPECT_EQ(*params->phDevice, EvictMemory_args.hDevice1);
            EXPECT_EQ(*params->pptr, EvictMemory_args.ptr1);
            EXPECT_EQ(*params->psize, EvictMemory_args.size1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Allocate instance data and pass to corresponding epilog
    //
    prologCbs3.Context.pfnEvictMemoryCb =
        [](ze_context_evict_memory_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(*params->phContext, EvictMemory_args.hContext1);
            EXPECT_EQ(*params->phDevice, EvictMemory_args.hDevice1);
            EXPECT_EQ(*params->pptr, EvictMemory_args.ptr1);
            EXPECT_EQ(*params->psize, EvictMemory_args.size1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = EvictMemory_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    epilogCbs3.Context.pfnEvictMemoryCb =
        [](ze_context_evict_memory_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(*params->phContext, EvictMemory_args.hContext1);
            EXPECT_EQ(*params->phDevice, EvictMemory_args.hDevice1);
            EXPECT_EQ(*params->pptr, EvictMemory_args.ptr1);
            EXPECT_EQ(*params->psize, EvictMemory_args.size1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, EvictMemory_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeContextEvictMemory_Tracing(EvictMemory_args.hContext0, EvictMemory_args.hDevice0, EvictMemory_args.ptr0, EvictMemory_args.size0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_context_handle_t hContext0;
    ze_device_handle_t hDevice0;
    ze_image_handle_t hImage0;
    ze_context_handle_t hContext1;
    ze_device_handle_t hDevice1;
    ze_image_handle_t hImage1;
    void *instanceData0;
    void *instanceData3;
} MakeImageResident_args;

TEST_F(zeAPITracingRuntimeMultipleArgumentsTests,
       WhenCallingContextMakeImageResidentTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    MakeImageResident_args.hContext0 = generateRandomHandle<ze_context_handle_t>();
    MakeImageResident_args.hDevice0 = generateRandomHandle<ze_device_handle_t>();
    MakeImageResident_args.hImage0 = generateRandomHandle<ze_image_handle_t>();

    // initialize replacement argument set
    MakeImageResident_args.hContext1 = generateRandomHandle<ze_context_handle_t>();
    MakeImageResident_args.hDevice1 = generateRandomHandle<ze_device_handle_t>();
    MakeImageResident_args.hImage1 = generateRandomHandle<ze_image_handle_t>();

    // initialize user instance data
    MakeImageResident_args.instanceData0 = generateRandomHandle<void *>();
    MakeImageResident_args.instanceData3 = generateRandomHandle<void *>();

    // arguments are expeted to be passed in from first prolog callback
    driver_ddiTable.core_ddiTable.Context.pfnMakeImageResident =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_image_handle_t hImage) {
            EXPECT_EQ(hContext, MakeImageResident_args.hContext1);
            EXPECT_EQ(hDevice, MakeImageResident_args.hDevice1);
            EXPECT_EQ(hImage, MakeImageResident_args.hImage1);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Allocate instance data, pass it to corresponding epilog.
    //
    prologCbs0.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(*params->phContext, MakeImageResident_args.hContext0);
            EXPECT_EQ(*params->phDevice, MakeImageResident_args.hDevice0);
            EXPECT_EQ(*params->phImage, MakeImageResident_args.hImage0);
            *params->phContext = MakeImageResident_args.hContext1;
            *params->phDevice = MakeImageResident_args.hDevice1;
            *params->phImage = MakeImageResident_args.hImage1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = MakeImageResident_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(*params->phContext, MakeImageResident_args.hContext1);
            EXPECT_EQ(*params->phDevice, MakeImageResident_args.hDevice1);
            EXPECT_EQ(*params->phImage, MakeImageResident_args.hImage1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, MakeImageResident_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(*params->phContext, MakeImageResident_args.hContext1);
            EXPECT_EQ(*params->phDevice, MakeImageResident_args.hDevice1);
            EXPECT_EQ(*params->phImage, MakeImageResident_args.hImage1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(*params->phContext, MakeImageResident_args.hContext1);
            EXPECT_EQ(*params->phDevice, MakeImageResident_args.hDevice1);
            EXPECT_EQ(*params->phImage, MakeImageResident_args.hImage1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Allocate instance data and pass to corresponding epilog
    //
    prologCbs3.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(*params->phContext, MakeImageResident_args.hContext1);
            EXPECT_EQ(*params->phDevice, MakeImageResident_args.hDevice1);
            EXPECT_EQ(*params->phImage, MakeImageResident_args.hImage1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = MakeImageResident_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    epilogCbs3.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(*params->phDevice, MakeImageResident_args.hDevice1);
            EXPECT_EQ(*params->phContext, MakeImageResident_args.hContext1);
            EXPECT_EQ(*params->phImage, MakeImageResident_args.hImage1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, MakeImageResident_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeContextMakeImageResident_Tracing(MakeImageResident_args.hContext0, MakeImageResident_args.hDevice0, MakeImageResident_args.hImage0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_context_handle_t hContext0;
    ze_device_handle_t hDevice0;
    ze_image_handle_t hImage0;
    ze_context_handle_t hContext1;
    ze_device_handle_t hDevice1;
    ze_image_handle_t hImage1;
    void *instanceData0;
    void *instanceData3;
} EvictImage_args;

TEST_F(zeAPITracingRuntimeMultipleArgumentsTests,
       WhenCallingContextMakeImageResidentTracingWrapperWithMultiplefPrologEpilogsThenReturnSuccess) {
    ze_result_t result;

    // initialize initial argument set
    EvictImage_args.hContext0 = generateRandomHandle<ze_context_handle_t>();
    EvictImage_args.hDevice0 = generateRandomHandle<ze_device_handle_t>();
    EvictImage_args.hImage0 = generateRandomHandle<ze_image_handle_t>();

    // initialize replacement argument set
    EvictImage_args.hContext1 = generateRandomHandle<ze_context_handle_t>();
    EvictImage_args.hDevice1 = generateRandomHandle<ze_device_handle_t>();
    EvictImage_args.hImage1 = generateRandomHandle<ze_image_handle_t>();

    // initialize user instance data
    EvictImage_args.instanceData0 = generateRandomHandle<void *>();
    EvictImage_args.instanceData3 = generateRandomHandle<void *>();

    // arguments are expeted to be passed in from first prolog callback
    driver_ddiTable.core_ddiTable.Context.pfnMakeImageResident =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_image_handle_t hImage) {
            EXPECT_EQ(hContext, EvictImage_args.hContext1);
            EXPECT_EQ(hDevice, EvictImage_args.hDevice1);
            EXPECT_EQ(hImage, EvictImage_args.hImage1);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Allocate instance data, pass it to corresponding epilog.
    //
    prologCbs0.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(*params->phContext, EvictImage_args.hContext0);
            EXPECT_EQ(*params->phDevice, EvictImage_args.hDevice0);
            EXPECT_EQ(*params->phImage, EvictImage_args.hImage0);
            *params->phContext = EvictImage_args.hContext1;
            *params->phDevice = EvictImage_args.hDevice1;
            *params->phImage = EvictImage_args.hImage1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = EvictImage_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(*params->phContext, EvictImage_args.hContext1);
            EXPECT_EQ(*params->phDevice, EvictImage_args.hDevice1);
            EXPECT_EQ(*params->phImage, EvictImage_args.hImage1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, EvictImage_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(*params->phContext, EvictImage_args.hContext1);
            EXPECT_EQ(*params->phDevice, EvictImage_args.hDevice1);
            EXPECT_EQ(*params->phImage, EvictImage_args.hImage1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(*params->phContext, EvictImage_args.hContext1);
            EXPECT_EQ(*params->phDevice, EvictImage_args.hDevice1);
            EXPECT_EQ(*params->phImage, EvictImage_args.hImage1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Allocate instance data and pass to corresponding epilog
    //
    prologCbs3.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(*params->phContext, EvictImage_args.hContext1);
            EXPECT_EQ(*params->phDevice, EvictImage_args.hDevice1);
            EXPECT_EQ(*params->phImage, EvictImage_args.hImage1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = EvictImage_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    epilogCbs3.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(*params->phContext, EvictImage_args.hContext1);
            EXPECT_EQ(*params->phDevice, EvictImage_args.hDevice1);
            EXPECT_EQ(*params->phImage, EvictImage_args.hImage1);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, EvictImage_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeContextMakeImageResident_Tracing(EvictImage_args.hContext0, EvictImage_args.hDevice0, EvictImage_args.hImage0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

} // namespace ult
} // namespace L0
