/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingContextCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Context.pfnCreate = [](ze_driver_handle_t hContext, const ze_context_desc_t *desc, ze_context_handle_t *phContext) -> ze_result_t { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextCreateTracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingContextDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Context.pfnDestroy = [](ze_context_handle_t hContext) -> ze_result_t { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextDestroyTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingContextGetStatusTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Context.pfnGetStatus = [](ze_context_handle_t hContext) -> ze_result_t { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnGetStatusCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnGetStatusCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextGetStatusTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingContextSystemBarrierTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Context.pfnSystemBarrier = [](ze_context_handle_t hContext, ze_device_handle_t hDevice) -> ze_result_t { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnSystemBarrierCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnSystemBarrierCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextSystemBarrierTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingContextMakeMemoryResidentTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Context.pfnMakeMemoryResident = [](ze_context_handle_t hContext, ze_device_handle_t hDevice, void *ptr, size_t size) -> ze_result_t { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnMakeMemoryResidentCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnMakeMemoryResidentCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextMakeMemoryResidentTracing(nullptr, nullptr, nullptr, 1024);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingContextEvictMemoryTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Context.pfnEvictMemory = [](ze_context_handle_t hContext, ze_device_handle_t hDevice, void *ptr, size_t size) -> ze_result_t { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnEvictMemoryCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnEvictMemoryCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextEvictMemoryTracing(nullptr, nullptr, nullptr, 1024);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingContextMakeImageResidentTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Context.pfnMakeImageResident = [](ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_image_handle_t hImage) -> ze_result_t { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnMakeImageResidentCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnMakeImageResidentCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextMakeImageResidentTracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingContextEvictImageTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Context.pfnEvictImage = [](ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_image_handle_t hImage) -> ze_result_t { return ZE_RESULT_SUCCESS; };

    prologCbs.Context.pfnEvictImageCb = genericPrologCallbackPtr;
    epilogCbs.Context.pfnEvictImageCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeContextEvictImageTracing(nullptr, nullptr, nullptr);
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
} makeMemoryResidentArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests,
       WhenCallingContextMakeMemoryResidentTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    makeMemoryResidentArgs.hContext0 = generateRandomHandle<ze_context_handle_t>();
    makeMemoryResidentArgs.hDevice0 = generateRandomHandle<ze_device_handle_t>();
    makeMemoryResidentArgs.ptr0 = generateRandomHandle<void *>();
    makeMemoryResidentArgs.size0 = generateRandomSize<size_t>();

    // initialize replacement argument set
    makeMemoryResidentArgs.hContext1 = generateRandomHandle<ze_context_handle_t>();
    makeMemoryResidentArgs.hDevice1 = generateRandomHandle<ze_device_handle_t>();
    makeMemoryResidentArgs.ptr1 = generateRandomHandle<void *>();
    makeMemoryResidentArgs.size1 = generateRandomSize<size_t>();

    // initialize user instance data
    makeMemoryResidentArgs.instanceData0 = generateRandomHandle<void *>();
    makeMemoryResidentArgs.instanceData3 = generateRandomHandle<void *>();

    // arguments are expeted to be passed in from first prolog callback
    driverDdiTable.coreDdiTable.Context.pfnMakeMemoryResident =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, void *ptr, size_t size) -> ze_result_t {
        EXPECT_EQ(hContext, makeMemoryResidentArgs.hContext1);
        EXPECT_EQ(hDevice, makeMemoryResidentArgs.hDevice1);
        EXPECT_EQ(ptr, makeMemoryResidentArgs.ptr1);
        EXPECT_EQ(size, makeMemoryResidentArgs.size1);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Allocate instance data, pass it to corresponding epilog.
    //
    prologCbs0.Context.pfnMakeMemoryResidentCb =
        [](ze_context_make_memory_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(*params->phContext, makeMemoryResidentArgs.hContext0);
        EXPECT_EQ(*params->phDevice, makeMemoryResidentArgs.hDevice0);
        EXPECT_EQ(*params->pptr, makeMemoryResidentArgs.ptr0);
        EXPECT_EQ(*params->psize, makeMemoryResidentArgs.size0);
        *params->phContext = makeMemoryResidentArgs.hContext1;
        *params->phDevice = makeMemoryResidentArgs.hDevice1;
        *params->pptr = makeMemoryResidentArgs.ptr1;
        *params->psize = makeMemoryResidentArgs.size1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = makeMemoryResidentArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Context.pfnMakeMemoryResidentCb =
        [](ze_context_make_memory_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(*params->phContext, makeMemoryResidentArgs.hContext1);
        EXPECT_EQ(*params->phDevice, makeMemoryResidentArgs.hDevice1);
        EXPECT_EQ(*params->pptr, makeMemoryResidentArgs.ptr1);
        EXPECT_EQ(*params->psize, makeMemoryResidentArgs.size1);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, makeMemoryResidentArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Context.pfnMakeMemoryResidentCb =
        [](ze_context_make_memory_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(*params->phContext, makeMemoryResidentArgs.hContext1);
        EXPECT_EQ(*params->phDevice, makeMemoryResidentArgs.hDevice1);
        EXPECT_EQ(*params->pptr, makeMemoryResidentArgs.ptr1);
        EXPECT_EQ(*params->psize, makeMemoryResidentArgs.size1);
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
        [](ze_context_make_memory_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(*params->phContext, makeMemoryResidentArgs.hContext1);
        EXPECT_EQ(*params->phDevice, makeMemoryResidentArgs.hDevice1);
        EXPECT_EQ(*params->pptr, makeMemoryResidentArgs.ptr1);
        EXPECT_EQ(*params->psize, makeMemoryResidentArgs.size1);
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
        [](ze_context_make_memory_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(*params->phContext, makeMemoryResidentArgs.hContext1);
        EXPECT_EQ(*params->phDevice, makeMemoryResidentArgs.hDevice1);
        EXPECT_EQ(*params->pptr, makeMemoryResidentArgs.ptr1);
        EXPECT_EQ(*params->psize, makeMemoryResidentArgs.size1);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = makeMemoryResidentArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    epilogCbs3.Context.pfnMakeMemoryResidentCb =
        [](ze_context_make_memory_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(*params->phContext, makeMemoryResidentArgs.hContext1);
        EXPECT_EQ(*params->phDevice, makeMemoryResidentArgs.hDevice1);
        EXPECT_EQ(*params->pptr, makeMemoryResidentArgs.ptr1);
        EXPECT_EQ(*params->psize, makeMemoryResidentArgs.size1);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, makeMemoryResidentArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeContextMakeMemoryResidentTracing(makeMemoryResidentArgs.hContext0, makeMemoryResidentArgs.hDevice0, makeMemoryResidentArgs.ptr0, makeMemoryResidentArgs.size0);
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
} evictMemoryArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingContextEvictMemoryTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    evictMemoryArgs.hContext0 = generateRandomHandle<ze_context_handle_t>();
    evictMemoryArgs.hDevice0 = generateRandomHandle<ze_device_handle_t>();
    evictMemoryArgs.ptr0 = generateRandomHandle<void *>();
    evictMemoryArgs.size0 = generateRandomSize<size_t>();

    // initialize replacement argument set
    evictMemoryArgs.hContext1 = generateRandomHandle<ze_context_handle_t>();
    evictMemoryArgs.hDevice1 = generateRandomHandle<ze_device_handle_t>();
    evictMemoryArgs.ptr1 = generateRandomHandle<void *>();
    evictMemoryArgs.size1 = generateRandomSize<size_t>();

    // initialize user instance data
    evictMemoryArgs.instanceData0 = generateRandomHandle<void *>();
    evictMemoryArgs.instanceData3 = generateRandomHandle<void *>();

    driverDdiTable.coreDdiTable.Context.pfnEvictMemory =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, void *ptr, size_t size) -> ze_result_t {
        EXPECT_EQ(hContext, evictMemoryArgs.hContext1);
        EXPECT_EQ(hDevice, evictMemoryArgs.hDevice1);
        EXPECT_EQ(ptr, evictMemoryArgs.ptr1);
        EXPECT_EQ(size, evictMemoryArgs.size1);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Context.pfnEvictMemoryCb =
        [](ze_context_evict_memory_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(evictMemoryArgs.hContext0, *params->phContext);
        EXPECT_EQ(evictMemoryArgs.hDevice0, *params->phDevice);
        EXPECT_EQ(evictMemoryArgs.ptr0, *params->pptr);
        EXPECT_EQ(evictMemoryArgs.size0, *params->psize);

        *params->phContext = evictMemoryArgs.hContext1;
        *params->phDevice = evictMemoryArgs.hDevice1;
        *params->pptr = evictMemoryArgs.ptr1;
        *params->psize = evictMemoryArgs.size1;

        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = evictMemoryArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Context.pfnEvictMemoryCb =
        [](ze_context_evict_memory_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(*params->phContext, evictMemoryArgs.hContext1);
        EXPECT_EQ(*params->phDevice, evictMemoryArgs.hDevice1);
        EXPECT_EQ(*params->pptr, evictMemoryArgs.ptr1);
        EXPECT_EQ(*params->psize, evictMemoryArgs.size1);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, evictMemoryArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Context.pfnEvictMemoryCb =
        [](ze_context_evict_memory_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(*params->phContext, evictMemoryArgs.hContext1);
        EXPECT_EQ(*params->phDevice, evictMemoryArgs.hDevice1);
        EXPECT_EQ(*params->pptr, evictMemoryArgs.ptr1);
        EXPECT_EQ(*params->psize, evictMemoryArgs.size1);
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
        [](ze_context_evict_memory_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(*params->phContext, evictMemoryArgs.hContext1);
        EXPECT_EQ(*params->phDevice, evictMemoryArgs.hDevice1);
        EXPECT_EQ(*params->pptr, evictMemoryArgs.ptr1);
        EXPECT_EQ(*params->psize, evictMemoryArgs.size1);
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
        [](ze_context_evict_memory_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(*params->phContext, evictMemoryArgs.hContext1);
        EXPECT_EQ(*params->phDevice, evictMemoryArgs.hDevice1);
        EXPECT_EQ(*params->pptr, evictMemoryArgs.ptr1);
        EXPECT_EQ(*params->psize, evictMemoryArgs.size1);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = evictMemoryArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    epilogCbs3.Context.pfnEvictMemoryCb =
        [](ze_context_evict_memory_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(*params->phContext, evictMemoryArgs.hContext1);
        EXPECT_EQ(*params->phDevice, evictMemoryArgs.hDevice1);
        EXPECT_EQ(*params->pptr, evictMemoryArgs.ptr1);
        EXPECT_EQ(*params->psize, evictMemoryArgs.size1);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, evictMemoryArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeContextEvictMemoryTracing(evictMemoryArgs.hContext0, evictMemoryArgs.hDevice0, evictMemoryArgs.ptr0, evictMemoryArgs.size0);
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
} makeImageResidentArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests,
       WhenCallingContextMakeImageResidentTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    makeImageResidentArgs.hContext0 = generateRandomHandle<ze_context_handle_t>();
    makeImageResidentArgs.hDevice0 = generateRandomHandle<ze_device_handle_t>();
    makeImageResidentArgs.hImage0 = generateRandomHandle<ze_image_handle_t>();

    // initialize replacement argument set
    makeImageResidentArgs.hContext1 = generateRandomHandle<ze_context_handle_t>();
    makeImageResidentArgs.hDevice1 = generateRandomHandle<ze_device_handle_t>();
    makeImageResidentArgs.hImage1 = generateRandomHandle<ze_image_handle_t>();

    // initialize user instance data
    makeImageResidentArgs.instanceData0 = generateRandomHandle<void *>();
    makeImageResidentArgs.instanceData3 = generateRandomHandle<void *>();

    // arguments are expeted to be passed in from first prolog callback
    driverDdiTable.coreDdiTable.Context.pfnMakeImageResident =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_image_handle_t hImage) -> ze_result_t {
        EXPECT_EQ(hContext, makeImageResidentArgs.hContext1);
        EXPECT_EQ(hDevice, makeImageResidentArgs.hDevice1);
        EXPECT_EQ(hImage, makeImageResidentArgs.hImage1);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Allocate instance data, pass it to corresponding epilog.
    //
    prologCbs0.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(*params->phContext, makeImageResidentArgs.hContext0);
        EXPECT_EQ(*params->phDevice, makeImageResidentArgs.hDevice0);
        EXPECT_EQ(*params->phImage, makeImageResidentArgs.hImage0);
        *params->phContext = makeImageResidentArgs.hContext1;
        *params->phDevice = makeImageResidentArgs.hDevice1;
        *params->phImage = makeImageResidentArgs.hImage1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = makeImageResidentArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(*params->phContext, makeImageResidentArgs.hContext1);
        EXPECT_EQ(*params->phDevice, makeImageResidentArgs.hDevice1);
        EXPECT_EQ(*params->phImage, makeImageResidentArgs.hImage1);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, makeImageResidentArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(*params->phContext, makeImageResidentArgs.hContext1);
        EXPECT_EQ(*params->phDevice, makeImageResidentArgs.hDevice1);
        EXPECT_EQ(*params->phImage, makeImageResidentArgs.hImage1);
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
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(*params->phContext, makeImageResidentArgs.hContext1);
        EXPECT_EQ(*params->phDevice, makeImageResidentArgs.hDevice1);
        EXPECT_EQ(*params->phImage, makeImageResidentArgs.hImage1);
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
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(*params->phContext, makeImageResidentArgs.hContext1);
        EXPECT_EQ(*params->phDevice, makeImageResidentArgs.hDevice1);
        EXPECT_EQ(*params->phImage, makeImageResidentArgs.hImage1);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = makeImageResidentArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    epilogCbs3.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(*params->phDevice, makeImageResidentArgs.hDevice1);
        EXPECT_EQ(*params->phContext, makeImageResidentArgs.hContext1);
        EXPECT_EQ(*params->phImage, makeImageResidentArgs.hImage1);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, makeImageResidentArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeContextMakeImageResidentTracing(makeImageResidentArgs.hContext0, makeImageResidentArgs.hDevice0, makeImageResidentArgs.hImage0);
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
} evictImageArgs;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests,
       WhenCallingContextMakeImageResidentTracingWrapperWithMultiplefPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    evictImageArgs.hContext0 = generateRandomHandle<ze_context_handle_t>();
    evictImageArgs.hDevice0 = generateRandomHandle<ze_device_handle_t>();
    evictImageArgs.hImage0 = generateRandomHandle<ze_image_handle_t>();

    // initialize replacement argument set
    evictImageArgs.hContext1 = generateRandomHandle<ze_context_handle_t>();
    evictImageArgs.hDevice1 = generateRandomHandle<ze_device_handle_t>();
    evictImageArgs.hImage1 = generateRandomHandle<ze_image_handle_t>();

    // initialize user instance data
    evictImageArgs.instanceData0 = generateRandomHandle<void *>();
    evictImageArgs.instanceData3 = generateRandomHandle<void *>();

    // arguments are expeted to be passed in from first prolog callback
    driverDdiTable.coreDdiTable.Context.pfnMakeImageResident =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_image_handle_t hImage) -> ze_result_t {
        EXPECT_EQ(hContext, evictImageArgs.hContext1);
        EXPECT_EQ(hDevice, evictImageArgs.hDevice1);
        EXPECT_EQ(hImage, evictImageArgs.hImage1);
        return ZE_RESULT_SUCCESS;
    };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Allocate instance data, pass it to corresponding epilog.
    //
    prologCbs0.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(*params->phContext, evictImageArgs.hContext0);
        EXPECT_EQ(*params->phDevice, evictImageArgs.hDevice0);
        EXPECT_EQ(*params->phImage, evictImageArgs.hImage0);
        *params->phContext = evictImageArgs.hContext1;
        *params->phDevice = evictImageArgs.hDevice1;
        *params->phImage = evictImageArgs.hImage1;
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 1);
        *val += 1;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = evictImageArgs.instanceData0;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(*params->phContext, evictImageArgs.hContext1);
        EXPECT_EQ(*params->phDevice, evictImageArgs.hDevice1);
        EXPECT_EQ(*params->phImage, evictImageArgs.hImage1);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 2);
        *val += 1;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, evictImageArgs.instanceData0);
        delete instanceData;
    };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(*params->phContext, evictImageArgs.hContext1);
        EXPECT_EQ(*params->phDevice, evictImageArgs.hDevice1);
        EXPECT_EQ(*params->phImage, evictImageArgs.hImage1);
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
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(*params->phContext, evictImageArgs.hContext1);
        EXPECT_EQ(*params->phDevice, evictImageArgs.hDevice1);
        EXPECT_EQ(*params->phImage, evictImageArgs.hImage1);
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
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        EXPECT_EQ(*params->phContext, evictImageArgs.hContext1);
        EXPECT_EQ(*params->phDevice, evictImageArgs.hDevice1);
        EXPECT_EQ(*params->phImage, evictImageArgs.hImage1);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 31);
        *val += 31;
        struct InstanceDataStruct *instanceData = new struct InstanceDataStruct;
        instanceData->instanceDataValue = evictImageArgs.instanceData3;
        *ppTracerInstanceUserData = instanceData;
    };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    epilogCbs3.Context.pfnMakeImageResidentCb =
        [](ze_context_make_image_resident_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) -> void {
        struct InstanceDataStruct *instanceData;
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(*params->phContext, evictImageArgs.hContext1);
        EXPECT_EQ(*params->phDevice, evictImageArgs.hDevice1);
        EXPECT_EQ(*params->phImage, evictImageArgs.hImage1);
        ASSERT_NE(nullptr, pTracerUserData);
        int *val = static_cast<int *>(pTracerUserData);
        EXPECT_EQ(*val, 62);
        *val += 31;
        instanceData = (struct InstanceDataStruct *)*ppTracerInstanceUserData;
        EXPECT_EQ(instanceData->instanceDataValue, evictImageArgs.instanceData3);
        delete instanceData;
    };

    setTracerCallbacksAndEnableTracer();

    result = zeContextMakeImageResidentTracing(evictImageArgs.hContext0, evictImageArgs.hDevice0, evictImageArgs.hImage0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

} // namespace ult
} // namespace L0
