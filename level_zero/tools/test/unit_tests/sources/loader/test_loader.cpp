/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/ddi/ze_ddi_tables.h"
#include <level_zero/zes_api.h>
#include <level_zero/zes_ddi.h>

namespace L0 {
namespace ult {

TEST(LoaderTest,
     whenCallingzesGetDriverProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_driver_dditable_t pDdiTable = {};

    ze_result_t result = zesGetDriverProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetDiagnosticsProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_diagnostics_dditable_t pDdiTable = {};

    ze_result_t result = zesGetDiagnosticsProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetEngineProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_engine_dditable_t pDdiTable = {};

    ze_result_t result = zesGetEngineProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetFabricPortProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {
    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_fabric_port_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFabricPortProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetFanProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_fan_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFanProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetDeviceProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_device_dditable_t pDdiTable = {};

    ze_result_t result = zesGetDeviceProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetFirmwareProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_firmware_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFirmwareProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetFrequencyProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_frequency_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFrequencyProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetLedProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_led_dditable_t pDdiTable = {};

    ze_result_t result = zesGetLedProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetMemoryProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_memory_dditable_t pDdiTable = {};

    ze_result_t result = zesGetMemoryProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetPerformanceFactorProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_performance_factor_dditable_t pDdiTable = {};

    ze_result_t result = zesGetPerformanceFactorProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetPowerProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_power_dditable_t pDdiTable = {};

    ze_result_t result = zesGetPowerProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetPsuProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_psu_dditable_t pDdiTable = {};

    ze_result_t result = zesGetPsuProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetRasProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_ras_dditable_t pDdiTable = {};

    ze_result_t result = zesGetRasProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetSchedulerProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_scheduler_dditable_t pDdiTable = {};

    ze_result_t result = zesGetSchedulerProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetStandbyProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = ZE_API_VERSION_1_0;
    zes_standby_dditable_t pDdiTable = {};

    ze_result_t result = zesGetStandbyProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(LoaderTest,
     whenCallingzesGetDriverProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_driver_dditable_t pDdiTable = {};

    ze_result_t result = zesGetDriverProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetDiagnosticsProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_diagnostics_dditable_t pDdiTable = {};

    ze_result_t result = zesGetDiagnosticsProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetEngineProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_engine_dditable_t pDdiTable = {};

    ze_result_t result = zesGetEngineProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetFabricPortProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_fabric_port_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFabricPortProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetFanProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_fan_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFanProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetDeviceProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_device_dditable_t pDdiTable = {};

    ze_result_t result = zesGetDeviceProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetFirmwareProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_firmware_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFirmwareProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetFrequencyProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_frequency_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFrequencyProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetLedProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_led_dditable_t pDdiTable = {};

    ze_result_t result = zesGetLedProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetMemoryProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_memory_dditable_t pDdiTable = {};

    ze_result_t result = zesGetMemoryProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetPerformanceFactorProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_performance_factor_dditable_t pDdiTable = {};

    ze_result_t result = zesGetPerformanceFactorProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetPowerProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_power_dditable_t pDdiTable = {};

    ze_result_t result = zesGetPowerProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetPsuProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_psu_dditable_t pDdiTable = {};

    ze_result_t result = zesGetPsuProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetRasProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_ras_dditable_t pDdiTable = {};

    ze_result_t result = zesGetRasProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetSchedulerProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_scheduler_dditable_t pDdiTable = {};

    ze_result_t result = zesGetSchedulerProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(LoaderTest,
     whenCallingzesGetStandbyProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
    zes_standby_dditable_t pDdiTable = {};

    ze_result_t result = zesGetStandbyProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

} // namespace ult
} // namespace L0
