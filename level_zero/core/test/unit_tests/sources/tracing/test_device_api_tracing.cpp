/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceGetTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnGet =
        [](ze_driver_handle_t hDriver, uint32_t *pCount, ze_device_handle_t *phDevices) { return ZE_RESULT_SUCCESS; };

    prologCbs.Device.pfnGetCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceGetTracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceGetPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnGetProperties =
        [](ze_device_handle_t hDevice, ze_device_properties_t *pDeviceProperties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Device.pfnGetPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceGetPropertiesTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceGetComputePropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnGetComputeProperties =
        [](ze_device_handle_t hDevice, ze_device_compute_properties_t *pComputeProperties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Device.pfnGetComputePropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetComputePropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceGetComputePropertiesTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceGetMemoryPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnGetMemoryProperties =
        [](ze_device_handle_t hDevice, uint32_t *pCount, ze_device_memory_properties_t *pMemProperties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Device.pfnGetMemoryPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetMemoryPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceGetMemoryPropertiesTracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceGetCachePropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnGetCacheProperties =
        [](ze_device_handle_t hDevice,
           uint32_t *pCount,
           ze_device_cache_properties_t *pCacheProperties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Device.pfnGetCachePropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetCachePropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceGetCachePropertiesTracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceGetImagePropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnGetImageProperties =
        [](ze_device_handle_t hDevice,
           ze_device_image_properties_t *pImageProperties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Device.pfnGetImagePropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetImagePropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceGetImagePropertiesTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceGetSubDevicesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnGetSubDevices =
        [](ze_device_handle_t hDevice,
           uint32_t *pCount,
           ze_device_handle_t *phSubdevices) { return ZE_RESULT_SUCCESS; };

    uint32_t pcount = 1;

    prologCbs.Device.pfnGetSubDevicesCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetSubDevicesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceGetSubDevicesTracing(nullptr, &pcount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceGetP2PPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnGetP2PProperties =
        [](ze_device_handle_t hDevice,
           ze_device_handle_t hPeerDevice,
           ze_device_p2p_properties_t *pP2PProperties) { return ZE_RESULT_SUCCESS; };

    ze_device_p2p_properties_t pP2PProperties;

    prologCbs.Device.pfnGetP2PPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetP2PPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceGetP2PPropertiesTracing(nullptr, nullptr, &pP2PProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceCanAccessPeerTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnCanAccessPeer =
        [](ze_device_handle_t hDevice,
           ze_device_handle_t hPeerDevice,
           ze_bool_t *value) { return ZE_RESULT_SUCCESS; };

    ze_bool_t value;

    prologCbs.Device.pfnCanAccessPeerCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnCanAccessPeerCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceCanAccessPeerTracing(nullptr, nullptr, &value);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingKernelSetCacheConfigTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Kernel.pfnSetCacheConfig =
        [](ze_kernel_handle_t hKernel,
           ze_cache_config_flags_t flags) { return ZE_RESULT_SUCCESS; };

    ze_cache_config_flags_t flags = {};

    prologCbs.Kernel.pfnSetCacheConfigCb = genericPrologCallbackPtr;
    epilogCbs.Kernel.pfnSetCacheConfigCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeKernelSetCacheConfigTracing(nullptr, flags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceGetModulePropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnGetModuleProperties =
        [](ze_device_handle_t hDevice,
           ze_device_module_properties_t *pModuleProperties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Device.pfnGetModulePropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetModulePropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceGetModulePropertiesTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceGetMemoryAccessPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnGetMemoryAccessProperties =
        [](ze_device_handle_t hDevice,
           ze_device_memory_access_properties_t *pMemAccessProperties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Device.pfnGetMemoryAccessPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetMemoryAccessPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceGetMemoryAccessPropertiesTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceGetCommandQueueGroupPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnGetCommandQueueGroupProperties =
        [](ze_device_handle_t hDevice, uint32_t *pCount, ze_command_queue_group_properties_t *pCommandQueueGroupProperties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Device.pfnGetCommandQueueGroupPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetCommandQueueGroupPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceGetCommandQueueGroupPropertiesTracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceGetExternalMemoryPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnGetExternalMemoryProperties =
        [](ze_device_handle_t hDevice, ze_device_external_memory_properties_t *pExternalMemoryProperties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Device.pfnGetExternalMemoryPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetExternalMemoryPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceGetExternalMemoryPropertiesTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingDeviceGetStatusTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Device.pfnGetStatus =
        [](ze_device_handle_t hDevice) { return ZE_RESULT_SUCCESS; };

    prologCbs.Device.pfnGetStatusCb = genericPrologCallbackPtr;
    epilogCbs.Device.pfnGetStatusCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeDeviceGetStatusTracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

} // namespace ult
} // namespace L0
