/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

#include <string.h>

namespace L0 {
namespace ult {

struct {
    ze_context_handle_t hContext0;
    ze_device_handle_t hDevice0;
    ze_module_desc_t desc0;
    ze_module_handle_t hModule0;
    ze_module_build_log_handle_t hBuildLog0;
    ze_context_handle_t hContext1;
    ze_device_handle_t hDevice1;
    ze_module_desc_t desc1;
    ze_module_handle_t hModule1;
    ze_module_build_log_handle_t hBuildLog1;
    ze_module_handle_t hModuleAPI;
    ze_module_build_log_handle_t hBuildLogAPI;
    void *instanceData0;
    void *instanceData3;
} module_create_args;

static void moduleCreateDescInitRandom(ze_module_desc_t *desc) {
    uint8_t *ptr = (uint8_t *)desc;
    for (size_t i = 0; i < sizeof(*desc); i++, ptr++) {
        *ptr = generateRandomSize<uint8_t>();
    }
}

static bool moduleCreateDescCompare(const ze_module_desc_t *phIpc0, const ze_module_desc_t *phIpc1) {
    if (nullptr == phIpc0) {
        return false;
    }
    if (nullptr == phIpc1) {
        return false;
    }
    return (memcmp((void *)phIpc0, (void *)phIpc1, sizeof(ze_module_desc_t)) == 0);
}

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingModuleCreateTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    module_create_args.hContext0 = generateRandomHandle<ze_context_handle_t>();
    module_create_args.hDevice0 = generateRandomHandle<ze_device_handle_t>();
    moduleCreateDescInitRandom(&module_create_args.desc0);
    module_create_args.hModule0 = generateRandomHandle<ze_module_handle_t>();
    module_create_args.hBuildLog0 = generateRandomHandle<ze_module_build_log_handle_t>();

    // initialize replacement argument set
    module_create_args.hContext1 = generateRandomHandle<ze_context_handle_t>();
    module_create_args.hDevice1 = generateRandomHandle<ze_device_handle_t>();
    moduleCreateDescInitRandom(&module_create_args.desc1);
    module_create_args.hModule1 = generateRandomHandle<ze_module_handle_t>();
    module_create_args.hBuildLog1 = generateRandomHandle<ze_module_build_log_handle_t>();

    // initialize user instance data
    module_create_args.instanceData0 = generateRandomHandle<void *>();
    module_create_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Module.pfnCreate =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, const ze_module_desc_t *desc, ze_module_handle_t *phModule, ze_module_build_log_handle_t *phBuildLog) {
            EXPECT_EQ(module_create_args.hContext1, hContext);
            EXPECT_EQ(module_create_args.hDevice1, hDevice);
            EXPECT_EQ(&module_create_args.desc1, desc);
            EXPECT_TRUE(moduleCreateDescCompare(&module_create_args.desc1, desc));
            EXPECT_EQ(&module_create_args.hModule1, phModule);
            EXPECT_EQ(module_create_args.hModule1, *phModule);
            EXPECT_EQ(&module_create_args.hBuildLog1, phBuildLog);
            EXPECT_EQ(module_create_args.hBuildLog1, *phBuildLog);
            module_create_args.hModuleAPI = generateRandomHandle<ze_module_handle_t>();
            module_create_args.hBuildLogAPI = generateRandomHandle<ze_module_build_log_handle_t>();
            *phModule = module_create_args.hModuleAPI;
            *phBuildLog = module_create_args.hBuildLogAPI;
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Module.pfnCreateCb =
        [](ze_module_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(module_create_args.hContext0, *params->phContext);
            EXPECT_EQ(module_create_args.hDevice0, *params->phDevice);
            EXPECT_EQ(&module_create_args.desc0, *params->pdesc);
            EXPECT_TRUE(moduleCreateDescCompare(&module_create_args.desc0, *params->pdesc));

            ze_module_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphModule;

            ze_module_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&module_create_args.hModule0, pHandle);

            ze_module_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(module_create_args.hModule0, handle);

            ze_module_build_log_handle_t **ppLogHandle;
            ppLogHandle = params->pphBuildLog;

            ze_module_build_log_handle_t *pLogHandle;
            ASSERT_NE(nullptr, ppLogHandle);
            pLogHandle = *ppLogHandle;
            EXPECT_EQ(&module_create_args.hBuildLog0, pLogHandle);

            ze_module_build_log_handle_t logHandle;
            logHandle = *pLogHandle;
            ASSERT_NE(nullptr, logHandle);
            EXPECT_EQ(module_create_args.hBuildLog0, logHandle);

            *params->phContext = module_create_args.hContext1;
            *params->phDevice = module_create_args.hDevice1;
            *params->pdesc = &module_create_args.desc1;
            *params->pphModule = &module_create_args.hModule1;
            *params->pphBuildLog = &module_create_args.hBuildLog1;

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = module_create_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Module.pfnCreateCb =
        [](ze_module_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(module_create_args.hContext1, *params->phContext);
            EXPECT_EQ(module_create_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&module_create_args.desc1, *params->pdesc);
            EXPECT_TRUE(moduleCreateDescCompare(&module_create_args.desc1, *params->pdesc));

            ze_module_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphModule;

            ze_module_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&module_create_args.hModule1, pHandle);

            ze_module_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(module_create_args.hModule1, handle);

            ze_module_build_log_handle_t **ppLogHandle;
            ppLogHandle = params->pphBuildLog;

            ze_module_build_log_handle_t *pLogHandle;
            ASSERT_NE(nullptr, ppLogHandle);
            pLogHandle = *ppLogHandle;
            EXPECT_EQ(&module_create_args.hBuildLog1, pLogHandle);

            ze_module_build_log_handle_t logHandle;
            logHandle = *pLogHandle;
            ASSERT_NE(nullptr, logHandle);
            EXPECT_EQ(module_create_args.hBuildLog1, logHandle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, module_create_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Module.pfnCreateCb =
        [](ze_module_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(module_create_args.hContext1, *params->phContext);
            EXPECT_EQ(module_create_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&module_create_args.desc1, *params->pdesc);
            EXPECT_TRUE(moduleCreateDescCompare(&module_create_args.desc1, *params->pdesc));

            ze_module_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphModule;

            ze_module_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&module_create_args.hModule1, pHandle);

            ze_module_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(module_create_args.hModule1, handle);

            ze_module_build_log_handle_t **ppLogHandle;
            ppLogHandle = params->pphBuildLog;

            ze_module_build_log_handle_t *pLogHandle;
            ASSERT_NE(nullptr, ppLogHandle);
            pLogHandle = *ppLogHandle;
            EXPECT_EQ(&module_create_args.hBuildLog1, pLogHandle);

            ze_module_build_log_handle_t logHandle;
            logHandle = *pLogHandle;
            ASSERT_NE(nullptr, logHandle);
            EXPECT_EQ(module_create_args.hBuildLog1, logHandle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Module.pfnCreateCb =
        [](ze_module_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(module_create_args.hContext1, *params->phContext);
            EXPECT_EQ(module_create_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&module_create_args.desc1, *params->pdesc);
            EXPECT_TRUE(moduleCreateDescCompare(&module_create_args.desc1, *params->pdesc));

            ze_module_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphModule;

            ze_module_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&module_create_args.hModule1, pHandle);

            ze_module_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(module_create_args.hModule1, handle);

            ze_module_build_log_handle_t **ppLogHandle;
            ppLogHandle = params->pphBuildLog;

            ze_module_build_log_handle_t *pLogHandle;
            ASSERT_NE(nullptr, ppLogHandle);
            pLogHandle = *ppLogHandle;
            EXPECT_EQ(&module_create_args.hBuildLog1, pLogHandle);

            ze_module_build_log_handle_t logHandle;
            logHandle = *pLogHandle;
            ASSERT_NE(nullptr, logHandle);
            EXPECT_EQ(module_create_args.hBuildLog1, logHandle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Module.pfnCreateCb =
        [](ze_module_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(module_create_args.hContext1, *params->phContext);
            EXPECT_EQ(module_create_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&module_create_args.desc1, *params->pdesc);
            EXPECT_TRUE(moduleCreateDescCompare(&module_create_args.desc1, *params->pdesc));

            ze_module_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphModule;

            ze_module_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&module_create_args.hModule1, pHandle);

            ze_module_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(module_create_args.hModule1, handle);

            ze_module_build_log_handle_t **ppLogHandle;
            ppLogHandle = params->pphBuildLog;

            ze_module_build_log_handle_t *pLogHandle;
            ASSERT_NE(nullptr, ppLogHandle);
            pLogHandle = *ppLogHandle;
            EXPECT_EQ(&module_create_args.hBuildLog1, pLogHandle);

            ze_module_build_log_handle_t logHandle;
            logHandle = *pLogHandle;
            ASSERT_NE(nullptr, logHandle);
            EXPECT_EQ(module_create_args.hBuildLog1, logHandle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = module_create_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Module.pfnCreateCb =
        [](ze_module_create_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(module_create_args.hContext1, *params->phContext);
            EXPECT_EQ(module_create_args.hDevice1, *params->phDevice);
            EXPECT_EQ(&module_create_args.desc1, *params->pdesc);
            EXPECT_TRUE(moduleCreateDescCompare(&module_create_args.desc1, *params->pdesc));

            ze_module_handle_t **ppHandle;
            ASSERT_NE(nullptr, params);
            ppHandle = params->pphModule;

            ze_module_handle_t *pHandle;
            ASSERT_NE(nullptr, ppHandle);
            pHandle = *ppHandle;
            EXPECT_EQ(&module_create_args.hModule1, pHandle);

            ze_module_handle_t handle;
            ASSERT_NE(nullptr, pHandle);
            handle = *pHandle;
            EXPECT_EQ(module_create_args.hModule1, handle);

            ze_module_build_log_handle_t **ppLogHandle;
            ppLogHandle = params->pphBuildLog;

            ze_module_build_log_handle_t *pLogHandle;
            ASSERT_NE(nullptr, ppLogHandle);
            pLogHandle = *ppLogHandle;
            EXPECT_EQ(&module_create_args.hBuildLog1, pLogHandle);

            ze_module_build_log_handle_t logHandle;
            logHandle = *pLogHandle;
            ASSERT_NE(nullptr, logHandle);
            EXPECT_EQ(module_create_args.hBuildLog1, logHandle);

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, module_create_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeModuleCreateTracing(module_create_args.hContext0, module_create_args.hDevice0, &module_create_args.desc0, &module_create_args.hModule0, &module_create_args.hBuildLog0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

struct {
    ze_module_handle_t hModule0;
    ze_module_handle_t hModule1;
    void *instanceData0;
    void *instanceData3;
} module_destroy_args;

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingModuleDestroyTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    module_destroy_args.hModule0 = generateRandomHandle<ze_module_handle_t>();

    // initialize replacement argument set
    module_destroy_args.hModule1 = generateRandomHandle<ze_module_handle_t>();

    // initialize user instance data
    module_destroy_args.instanceData0 = generateRandomHandle<void *>();
    module_destroy_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Module.pfnDestroy =
        [](ze_module_handle_t hModule) {
            EXPECT_EQ(module_destroy_args.hModule1, hModule);
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Module.pfnDestroyCb =
        [](ze_module_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(module_destroy_args.hModule0, *params->phModule);
            *params->phModule = module_destroy_args.hModule1;
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = module_destroy_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Module.pfnDestroyCb =
        [](ze_module_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(module_destroy_args.hModule1, *params->phModule);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, module_destroy_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Module.pfnDestroyCb =
        [](ze_module_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(module_destroy_args.hModule1, *params->phModule);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Module.pfnDestroyCb =
        [](ze_module_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(module_destroy_args.hModule1, *params->phModule);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Module.pfnDestroyCb =
        [](ze_module_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(module_destroy_args.hModule1, *params->phModule);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = module_destroy_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Module.pfnDestroyCb =
        [](ze_module_destroy_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(module_destroy_args.hModule1, *params->phModule);
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, module_destroy_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeModuleDestroyTracing(module_destroy_args.hModule0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

static constexpr size_t moduleGetNativeBinarySize0 = 64;
static constexpr size_t moduleGetNativeBinarySize1 = 128;

struct {
    ze_module_handle_t hModule0;
    size_t size0 = moduleGetNativeBinarySize0;
    uint8_t moduleNativeBinary0[moduleGetNativeBinarySize0];
    ze_module_handle_t hModule1;
    size_t size1 = moduleGetNativeBinarySize1;
    uint8_t moduleNativeBinary1[moduleGetNativeBinarySize1];
    void *instanceData0;
    void *instanceData3;
} module_get_native_binary_args;

static void moduleGetNativeBinaryNativeBinaryInitRandom(uint8_t *binary, size_t size) {
    uint8_t *ptr = binary;
    for (size_t i = 0; i < size; i++) {
        *ptr = generateRandomSize<uint8_t>();
    }
}

static bool moduleGetNativeBinaryNativeBinaryCompare(uint8_t *binary0, uint8_t *binary1, size_t size) {
    if (binary0 == nullptr) {
        return false;
    }
    if (binary1 == nullptr) {
        return false;
    }
    return (memcmp(static_cast<void *>(binary0), static_cast<void *>(binary1), size) == 0);
}

TEST_F(ZeApiTracingRuntimeMultipleArgumentsTests, WhenCallingModuleGetNativeBinaryTracingWrapperWithMultiplePrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    // initialize initial argument set
    module_get_native_binary_args.hModule0 = generateRandomHandle<ze_module_handle_t>();
    moduleGetNativeBinaryNativeBinaryInitRandom(module_get_native_binary_args.moduleNativeBinary0, moduleGetNativeBinarySize0);

    // initialize replacement argument set
    module_get_native_binary_args.hModule1 = generateRandomHandle<ze_module_handle_t>();
    moduleGetNativeBinaryNativeBinaryInitRandom(module_get_native_binary_args.moduleNativeBinary1, moduleGetNativeBinarySize1);

    // initialize user instance data
    module_get_native_binary_args.instanceData0 = generateRandomHandle<void *>();
    module_get_native_binary_args.instanceData3 = generateRandomHandle<void *>();

    driver_ddiTable.core_ddiTable.Module.pfnGetNativeBinary =
        [](ze_module_handle_t hModule, size_t *pSize, uint8_t *pModuleNativeBinary) {
            EXPECT_EQ(module_get_native_binary_args.hModule1, hModule);
            EXPECT_EQ(&module_get_native_binary_args.size1, pSize);
            EXPECT_EQ(*pSize, moduleGetNativeBinarySize1);
            EXPECT_EQ(module_get_native_binary_args.moduleNativeBinary1, pModuleNativeBinary);
            EXPECT_TRUE(moduleGetNativeBinaryNativeBinaryCompare(module_get_native_binary_args.moduleNativeBinary1,
                                                                 pModuleNativeBinary, moduleGetNativeBinarySize0));
            return ZE_RESULT_SUCCESS;
        };

    //
    // The 0th prolog replaces the orignal API arguments with a new set
    // Create instance data, pass it to corresponding epilog.
    //
    prologCbs0.Module.pfnGetNativeBinaryCb =
        [](ze_module_get_native_binary_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(module_get_native_binary_args.hModule0, *params->phModule);

            size_t **ppSize;
            ASSERT_NE(nullptr, params);
            ppSize = params->ppSize;

            size_t *pSize;
            ASSERT_NE(nullptr, ppSize);
            pSize = *ppSize;
            EXPECT_EQ(&module_get_native_binary_args.size0, *params->ppSize);

            size_t size;
            ASSERT_NE(nullptr, pSize);
            size = *pSize;
            EXPECT_EQ(size, moduleGetNativeBinarySize0);

            EXPECT_EQ(module_get_native_binary_args.moduleNativeBinary0, *params->ppModuleNativeBinary);
            EXPECT_TRUE(moduleGetNativeBinaryNativeBinaryCompare(module_get_native_binary_args.moduleNativeBinary0,
                                                                 *params->ppModuleNativeBinary, moduleGetNativeBinarySize0));
            *params->phModule = module_get_native_binary_args.hModule1;
            *params->ppSize = &module_get_native_binary_args.size1;
            *params->ppModuleNativeBinary = module_get_native_binary_args.moduleNativeBinary1;

            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 1);
            *val += 1;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = module_get_native_binary_args.instanceData0;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 0th epilog expects to see the API argument replacements
    // Expect to receive instance data from corresponding prolog
    //
    epilogCbs0.Module.pfnGetNativeBinaryCb =
        [](ze_module_get_native_binary_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(module_get_native_binary_args.hModule1, *params->phModule);

            size_t **ppSize;
            ASSERT_NE(nullptr, params);
            ppSize = params->ppSize;

            size_t *pSize;
            ASSERT_NE(nullptr, ppSize);
            pSize = *ppSize;
            EXPECT_EQ(&module_get_native_binary_args.size1, *params->ppSize);

            size_t size;
            ASSERT_NE(nullptr, pSize);
            size = *pSize;
            EXPECT_EQ(size, moduleGetNativeBinarySize1);

            EXPECT_EQ(module_get_native_binary_args.moduleNativeBinary1, *params->ppModuleNativeBinary);
            EXPECT_TRUE(moduleGetNativeBinaryNativeBinaryCompare(module_get_native_binary_args.moduleNativeBinary1,
                                                                 *params->ppModuleNativeBinary, moduleGetNativeBinarySize1));
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 2);
            *val += 1;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, module_get_native_binary_args.instanceData0);
            delete instanceData;
        };

    //
    // The 1st prolog sees the arguments as replaced by the 0th prolog.
    // There is no epilog for this prolog, so don't allocate instance data
    //
    prologCbs1.Module.pfnGetNativeBinaryCb =
        [](ze_module_get_native_binary_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(module_get_native_binary_args.hModule1, *params->phModule);

            size_t **ppSize;
            ASSERT_NE(nullptr, params);
            ppSize = params->ppSize;

            size_t *pSize;
            ASSERT_NE(nullptr, ppSize);
            pSize = *ppSize;
            EXPECT_EQ(&module_get_native_binary_args.size1, *params->ppSize);

            size_t size;
            ASSERT_NE(nullptr, pSize);
            size = *pSize;
            EXPECT_EQ(size, moduleGetNativeBinarySize1);

            EXPECT_EQ(module_get_native_binary_args.moduleNativeBinary1, *params->ppModuleNativeBinary);
            EXPECT_TRUE(moduleGetNativeBinaryNativeBinaryCompare(module_get_native_binary_args.moduleNativeBinary1,
                                                                 *params->ppModuleNativeBinary, moduleGetNativeBinarySize1));
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 11);
            *val += 11;
        };

    //
    // The 2nd epilog expects to see the API argument replacements
    // There is no corresponding prolog, so there is no instance data
    //
    epilogCbs2.Module.pfnGetNativeBinaryCb =
        [](ze_module_get_native_binary_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(module_get_native_binary_args.hModule1, *params->phModule);

            size_t **ppSize;
            ASSERT_NE(nullptr, params);
            ppSize = params->ppSize;

            size_t *pSize;
            ASSERT_NE(nullptr, ppSize);
            pSize = *ppSize;
            EXPECT_EQ(&module_get_native_binary_args.size1, *params->ppSize);

            size_t size;
            ASSERT_NE(nullptr, pSize);
            size = *pSize;
            EXPECT_EQ(size, moduleGetNativeBinarySize1);

            EXPECT_EQ(module_get_native_binary_args.moduleNativeBinary1, *params->ppModuleNativeBinary);
            EXPECT_TRUE(moduleGetNativeBinaryNativeBinaryCompare(module_get_native_binary_args.moduleNativeBinary1,
                                                                 *params->ppModuleNativeBinary, moduleGetNativeBinarySize1));
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 21);
            *val += 21;
        };

    //
    // The 3rd prolog expects to see the API argument replacements and doesn't modify them
    // Create instance data and pass to corresponding epilog
    //
    prologCbs3.Module.pfnGetNativeBinaryCb =
        [](ze_module_get_native_binary_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            EXPECT_EQ(module_get_native_binary_args.hModule1, *params->phModule);
            EXPECT_EQ(&module_get_native_binary_args.size1, *params->ppSize);

            size_t **ppSize;
            ASSERT_NE(nullptr, params);
            ppSize = params->ppSize;

            size_t *pSize;
            ASSERT_NE(nullptr, ppSize);
            pSize = *ppSize;
            EXPECT_EQ(&module_get_native_binary_args.size1, *params->ppSize);

            size_t size;
            ASSERT_NE(nullptr, pSize);
            size = *pSize;
            EXPECT_EQ(size, moduleGetNativeBinarySize1);

            EXPECT_EQ(module_get_native_binary_args.moduleNativeBinary1, *params->ppModuleNativeBinary);
            EXPECT_TRUE(moduleGetNativeBinaryNativeBinaryCompare(module_get_native_binary_args.moduleNativeBinary1,
                                                                 *params->ppModuleNativeBinary, moduleGetNativeBinarySize1));
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 31);
            *val += 31;
            struct instanceDataStruct *instanceData = new struct instanceDataStruct;
            instanceData->instanceDataValue = module_get_native_binary_args.instanceData3;
            *ppTracerInstanceUserData = instanceData;
        };

    //
    // The 3rd epilog expects to see the API argument replacements
    // Expect to see instance data from corresponding prolog
    //
    epilogCbs3.Module.pfnGetNativeBinaryCb =
        [](ze_module_get_native_binary_params_t *params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
            struct instanceDataStruct *instanceData;
            EXPECT_EQ(result, ZE_RESULT_SUCCESS);
            EXPECT_EQ(module_get_native_binary_args.hModule1, *params->phModule);

            size_t **ppSize;
            ASSERT_NE(nullptr, params);
            ppSize = params->ppSize;

            size_t *pSize;
            ASSERT_NE(nullptr, ppSize);
            pSize = *ppSize;
            EXPECT_EQ(&module_get_native_binary_args.size1, *params->ppSize);

            size_t size;
            ASSERT_NE(nullptr, pSize);
            size = *pSize;
            EXPECT_EQ(size, moduleGetNativeBinarySize1);

            EXPECT_EQ(module_get_native_binary_args.moduleNativeBinary1, *params->ppModuleNativeBinary);
            EXPECT_TRUE(moduleGetNativeBinaryNativeBinaryCompare(module_get_native_binary_args.moduleNativeBinary1,
                                                                 *params->ppModuleNativeBinary, moduleGetNativeBinarySize1));
            ASSERT_NE(nullptr, pTracerUserData);
            int *val = static_cast<int *>(pTracerUserData);
            EXPECT_EQ(*val, 62);
            *val += 31;
            instanceData = (struct instanceDataStruct *)*ppTracerInstanceUserData;
            EXPECT_EQ(instanceData->instanceDataValue, module_get_native_binary_args.instanceData3);
            delete instanceData;
        };

    setTracerCallbacksAndEnableTracer();

    result = zeModuleGetNativeBinaryTracing(module_get_native_binary_args.hModule0, &module_get_native_binary_args.size0, module_get_native_binary_args.moduleNativeBinary0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    validateDefaultUserDataFinal();
}

} // namespace ult
} // namespace L0
