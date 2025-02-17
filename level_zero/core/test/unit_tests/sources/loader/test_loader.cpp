/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/ddi/ze_ddi_tables.h"
#include <level_zero/ze_api.h>

namespace L0 {
namespace ult {

TEST(zeGetDriverProcAddrTableTest,
     whenCallingZeGetDriverProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_driver_dditable_t pDdiTable = {};

    ze_result_t result = zeGetDriverProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetMemProcAddrTableTest,
     whenCallingZeGetMemProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_mem_dditable_t pDdiTable = {};

    ze_result_t result = zeGetMemProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetContextProcAddrTableTest,
     whenCallingZeGetContextProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_context_dditable_t pDdiTable = {};

    ze_result_t result = zeGetContextProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetPhysicalMemProcAddrTableTest,
     whenCallingZeGetPhysicalMemProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_physical_mem_dditable_t pDdiTable = {};

    ze_result_t result = zeGetPhysicalMemProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetVirtualMemProcAddrTableTest,
     whenCallingZeGetVirtualMemProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_virtual_mem_dditable_t pDdiTable = {};

    ze_result_t result = zeGetVirtualMemProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetGlobalProcAddrTableTest,
     whenCallingZeGetGlobalProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_global_dditable_t pDdiTable = {};

    ze_result_t result = zeGetGlobalProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetDeviceProcAddrTableTest,
     whenCallingZeGetDeviceProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_device_dditable_t pDdiTable = {};

    ze_result_t result = zeGetDeviceProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetCommandQueueProcAddrTableTest,
     whenCallingZeGetCommandQueueProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_command_queue_dditable_t pDdiTable = {};

    ze_result_t result = zeGetCommandQueueProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetCommandListProcAddrTableTest,
     whenCallingZeGetCommandListProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_command_list_dditable_t pDdiTable = {};

    ze_result_t result = zeGetCommandListProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetFenceProcAddrTableTest,
     whenCallingZeGetFenceProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_fence_dditable_t pDdiTable = {};

    ze_result_t result = zeGetFenceProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetEventPoolProcAddrTableTest,
     whenCallingZeGetEventPoolProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_event_pool_dditable_t pDdiTable = {};

    ze_result_t result = zeGetEventPoolProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetEventProcAddrTableTest,
     whenCallingZeGetEventProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_event_dditable_t pDdiTable = {};

    ze_result_t result = zeGetEventProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetImageProcAddrTableTest,
     whenCallingZeGetImageProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_image_dditable_t pDdiTable = {};

    ze_result_t result = zeGetImageProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetModuleProcAddrTableTest,
     whenCallingZeGetModuleProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_module_dditable_t pDdiTable = {};

    ze_result_t result = zeGetModuleProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetModuleBuildLogProcAddrTableTest,
     whenCallingZeGetModuleBuildLogProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_module_build_log_dditable_t pDdiTable = {};

    ze_result_t result = zeGetModuleBuildLogProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetKernelProcAddrTableTest,
     whenCallingZeGetKernelProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_kernel_dditable_t pDdiTable = {};

    ze_result_t result = zeGetKernelProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetSamplerProcAddrTableTest,
     whenCallingZeGetSamplerProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    ze_sampler_dditable_t pDdiTable = {};

    ze_result_t result = zeGetSamplerProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeGetDriverProcAddrTableTest,
     whenCallingZeGetDriverProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_driver_dditable_t pDdiTable = {};

    ze_result_t result = zeGetDriverProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetMemProcAddrTableTest,
     whenCallingZeGetMemProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_mem_dditable_t pDdiTable = {};

    ze_result_t result = zeGetMemProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetContextProcAddrTableTest,
     whenCallingZeGetContextProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_context_dditable_t pDdiTable = {};

    ze_result_t result = zeGetContextProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetPhysicalMemProcAddrTableTest,
     whenCallingZeGetPhysicalMemProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_physical_mem_dditable_t pDdiTable = {};

    ze_result_t result = zeGetPhysicalMemProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetVirtualMemProcAddrTableTest,
     whenCallingZeGetVirtualMemProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_virtual_mem_dditable_t pDdiTable = {};

    ze_result_t result = zeGetVirtualMemProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetGlobalProcAddrTableTest,
     whenCallingZeGetGlobalProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_global_dditable_t pDdiTable = {};

    ze_result_t result = zeGetGlobalProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetDeviceProcAddrTableTest,
     whenCallingZeGetDeviceProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_device_dditable_t pDdiTable = {};

    ze_result_t result = zeGetDeviceProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetCommandQueueProcAddrTableTest,
     whenCallingZeGetCommandQueueProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_command_queue_dditable_t pDdiTable = {};

    ze_result_t result = zeGetCommandQueueProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetCommandListProcAddrTableTest,
     whenCallingZeGetCommandListProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_command_list_dditable_t pDdiTable = {};

    ze_result_t result = zeGetCommandListProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetFenceProcAddrTableTest,
     whenCallingZeGetFenceProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_fence_dditable_t pDdiTable = {};

    ze_result_t result = zeGetFenceProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetEventPoolProcAddrTableTest,
     whenCallingZeGetEventPoolProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_event_pool_dditable_t pDdiTable = {};

    ze_result_t result = zeGetEventPoolProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetEventProcAddrTableTest,
     whenCallingZeGetEventProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_event_dditable_t pDdiTable = {};

    ze_result_t result = zeGetEventProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetImageProcAddrTableTest,
     whenCallingZeGetImageProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_image_dditable_t pDdiTable = {};

    ze_result_t result = zeGetImageProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetModuleProcAddrTableTest,
     whenCallingZeGetModuleProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_module_dditable_t pDdiTable = {};

    ze_result_t result = zeGetModuleProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetModuleBuildLogProcAddrTableTest,
     whenCallingZeGetModuleBuildLogProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_module_build_log_dditable_t pDdiTable = {};

    ze_result_t result = zeGetModuleBuildLogProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetKernelProcAddrTableTest,
     whenCallingZeGetKernelProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_kernel_dditable_t pDdiTable = {};

    ze_result_t result = zeGetKernelProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zeGetSamplerProcAddrTableTest,
     whenCallingZeGetSamplerProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    ze_sampler_dditable_t pDdiTable = {};

    ze_result_t result = zeGetSamplerProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

} // namespace ult
} // namespace L0
