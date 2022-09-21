/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingzeDriverGetTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.Driver.pfnGet =
        [](uint32_t *pCount, ze_driver_handle_t *phDrivers) { return ZE_RESULT_SUCCESS; };

    prologCbs.Driver.pfnGetCb = genericPrologCallbackPtr;
    epilogCbs.Driver.pfnGetCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDriverGetTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingzeDriverGetPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.Driver.pfnGetProperties =
        [](ze_driver_handle_t hDriver, ze_driver_properties_t *properties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Device.pfnGetSubDevicesCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetSubDevicesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDriverGetPropertiesTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingzeDriverGetApiVersionTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.Driver.pfnGetApiVersion =
        [](ze_driver_handle_t hDrivers, ze_api_version_t *version) { return ZE_RESULT_SUCCESS; };

    prologCbs.Driver.pfnGetApiVersionCb = genericPrologCallbackPtr;
    epilogCbs.Driver.pfnGetApiVersionCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDriverGetApiVersionTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingzeDriverGetIpcPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.Driver.pfnGetIpcProperties =
        [](ze_driver_handle_t hDrivers,
           ze_driver_ipc_properties_t *pIpcProperties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Driver.pfnGetIpcPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Driver.pfnGetIpcPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDriverGetIpcPropertiesTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingzeDriverGetExtensionPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.Driver.pfnGetExtensionProperties =
        [](ze_driver_handle_t hDrivers,
           uint32_t *pCount,
           ze_driver_extension_properties_t *pExtensionProperties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Driver.pfnGetExtensionPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Driver.pfnGetExtensionPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDriverGetExtensionPropertiesTracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

} // namespace ult
} // namespace L0
