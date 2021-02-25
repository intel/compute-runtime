/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include <level_zero/zes_api.h>
#include <level_zero/zes_ddi.h>

#include "ze_ddi_tables.h"

namespace L0 {
namespace ult {

TEST(zesGetDriverProcAddrTableTest,
     whenCallingzesGetDriverProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_driver_dditable_t pDdiTable = {};

    ze_result_t result = zesGetDriverProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetDiagnosticsProcAddrTableTest,
     whenCallingzesGetDiagnosticsProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_diagnostics_dditable_t pDdiTable = {};

    ze_result_t result = zesGetDiagnosticsProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetEngineProcAddrTableTest,
     whenCallingzesGetEngineProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_engine_dditable_t pDdiTable = {};

    ze_result_t result = zesGetEngineProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetFabricPortProcAddrTableTest,
     whenCallingzesGetFabricPortProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_fabric_port_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFabricPortProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetFanProcAddrTableTest,
     whenCallingzesGetFanProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_fan_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFanProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetDeviceProcAddrTableTest,
     whenCallingzesGetDeviceProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_device_dditable_t pDdiTable = {};

    ze_result_t result = zesGetDeviceProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetFirmwareProcAddrTableTest,
     whenCallingzesGetFirmwareProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_firmware_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFirmwareProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetFrequencyProcAddrTableTest,
     whenCallingzesGetFrequencyProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_frequency_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFrequencyProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetLedProcAddrTableTest,
     whenCallingzesGetLedProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_led_dditable_t pDdiTable = {};

    ze_result_t result = zesGetLedProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetMemoryProcAddrTableTest,
     whenCallingzesGetMemoryProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_memory_dditable_t pDdiTable = {};

    ze_result_t result = zesGetMemoryProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetPerformanceFactorProcAddrTableTest,
     whenCallingzesGetPerformanceFactorProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_performance_factor_dditable_t pDdiTable = {};

    ze_result_t result = zesGetPerformanceFactorProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetPowerProcAddrTableTest,
     whenCallingzesGetPowerProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_power_dditable_t pDdiTable = {};

    ze_result_t result = zesGetPowerProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetPsuProcAddrTableTest,
     whenCallingzesGetPsuProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_psu_dditable_t pDdiTable = {};

    ze_result_t result = zesGetPsuProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetRasProcAddrTableTest,
     whenCallingzesGetRasProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_ras_dditable_t pDdiTable = {};

    ze_result_t result = zesGetRasProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetSchedulerProcAddrTableTest,
     whenCallingzesGetSchedulerProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_scheduler_dditable_t pDdiTable = {};

    ze_result_t result = zesGetSchedulerProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetStandbyProcAddrTableTest,
     whenCallingzesGetStandbyProcAddrTableWithCorrectMajorVersionThenSuccessIsReturnedAndMinorVersionIsIgnored) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1u, 64u));
    zes_standby_dditable_t pDdiTable = {};

    ze_result_t result = zesGetStandbyProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zesGetDriverProcAddrTableTest,
     whenCallingzesGetDriverProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_driver_dditable_t pDdiTable = {};

    ze_result_t result = zesGetDriverProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetDiagnosticsProcAddrTableTest,
     whenCallingzesGetDiagnosticsProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_diagnostics_dditable_t pDdiTable = {};

    ze_result_t result = zesGetDiagnosticsProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetEngineProcAddrTableTest,
     whenCallingzesGetEngineProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_engine_dditable_t pDdiTable = {};

    ze_result_t result = zesGetEngineProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetFabricPortProcAddrTableTest,
     whenCallingzesGetFabricPortProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_fabric_port_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFabricPortProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetFanProcAddrTableTest,
     whenCallingzesGetFanProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_fan_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFanProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetDeviceProcAddrTableTest,
     whenCallingzesGetDeviceProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_device_dditable_t pDdiTable = {};

    ze_result_t result = zesGetDeviceProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetFirmwareProcAddrTableTest,
     whenCallingzesGetFirmwareProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_firmware_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFirmwareProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetFrequencyProcAddrTableTest,
     whenCallingzesGetFrequencyProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_frequency_dditable_t pDdiTable = {};

    ze_result_t result = zesGetFrequencyProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetLedProcAddrTableTest,
     whenCallingzesGetLedProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_led_dditable_t pDdiTable = {};

    ze_result_t result = zesGetLedProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetMemoryProcAddrTableTest,
     whenCallingzesGetMemoryProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_memory_dditable_t pDdiTable = {};

    ze_result_t result = zesGetMemoryProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetPerformanceFactorProcAddrTableTest,
     whenCallingzesGetPerformanceFactorProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_performance_factor_dditable_t pDdiTable = {};

    ze_result_t result = zesGetPerformanceFactorProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetPowerProcAddrTableTest,
     whenCallingzesGetPowerProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_power_dditable_t pDdiTable = {};

    ze_result_t result = zesGetPowerProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetPsuProcAddrTableTest,
     whenCallingzesGetPsuProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_psu_dditable_t pDdiTable = {};

    ze_result_t result = zesGetPsuProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetRasProcAddrTableTest,
     whenCallingzesGetRasProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_ras_dditable_t pDdiTable = {};

    ze_result_t result = zesGetRasProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetSchedulerProcAddrTableTest,
     whenCallingzesGetSchedulerProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_scheduler_dditable_t pDdiTable = {};

    ze_result_t result = zesGetSchedulerProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

TEST(zesGetStandbyProcAddrTableTest,
     whenCallingzesGetStandbyProcAddrTableWithGreaterThanAllowedMajorVersionThenUnitializedIsReturned) {

    ze_api_version_t version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(64u, 0u));
    zes_standby_dditable_t pDdiTable = {};

    ze_result_t result = zesGetStandbyProcAddrTable(version, &pDdiTable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, result);
}

} // namespace ult
} // namespace L0
