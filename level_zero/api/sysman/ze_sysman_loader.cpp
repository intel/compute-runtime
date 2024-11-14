/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zes_api.h>
#include <level_zero/zes_ddi.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

#include "ze_ddi_tables.h"
#include "zes_sysman_all_api_entrypoints.h"

extern ze_gpu_driver_dditable_t driverDdiTable;

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetDeviceProcAddrTable(
    ze_api_version_t version,
    zes_device_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesDeviceGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::zesDeviceGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReset, L0::zesDeviceReset, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnProcessesGetState, L0::zesDeviceProcessesGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnPciGetProperties, L0::zesDevicePciGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnPciGetState, L0::zesDevicePciGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnPciGetBars, L0::zesDevicePciGetBars, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnPciGetStats, L0::zesDevicePciGetStats, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumDiagnosticTestSuites, L0::zesDeviceEnumDiagnosticTestSuites, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumEngineGroups, L0::zesDeviceEnumEngineGroups, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEventRegister, L0::zesDeviceEventRegister, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumFabricPorts, L0::zesDeviceEnumFabricPorts, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumFans, L0::zesDeviceEnumFans, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumFirmwares, L0::zesDeviceEnumFirmwares, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumFrequencyDomains, L0::zesDeviceEnumFrequencyDomains, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumLeds, L0::zesDeviceEnumLeds, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumMemoryModules, L0::zesDeviceEnumMemoryModules, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumPerformanceFactorDomains, L0::zesDeviceEnumPerformanceFactorDomains, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumPowerDomains, L0::zesDeviceEnumPowerDomains, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumPsus, L0::zesDeviceEnumPsus, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumRasErrorSets, L0::zesDeviceEnumRasErrorSets, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumSchedulers, L0::zesDeviceEnumSchedulers, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumStandbyDomains, L0::zesDeviceEnumStandbyDomains, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumTemperatureSensors, L0::zesDeviceEnumTemperatureSensors, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetCardPowerDomain, L0::zesDeviceGetCardPowerDomain, version, ZE_API_VERSION_1_3);
    fillDdiEntry(pDdiTable->pfnEccAvailable, L0::zesDeviceEccAvailable, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnEccConfigurable, L0::zesDeviceEccConfigurable, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGetEccState, L0::zesDeviceGetEccState, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnSetEccState, L0::zesDeviceSetEccState, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGet, L0::zesDeviceGet, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnSetOverclockWaiver, L0::zesDeviceSetOverclockWaiver, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetOverclockDomains, L0::zesDeviceGetOverclockDomains, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetOverclockControls, L0::zesDeviceGetOverclockControls, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnResetOverclockSettings, L0::zesDeviceResetOverclockSettings, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnReadOverclockState, L0::zesDeviceReadOverclockState, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnEnumOverclockDomains, L0::zesDeviceEnumOverclockDomains, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnResetExt, L0::zesDeviceResetExt, version, ZE_API_VERSION_1_7);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetGlobalProcAddrTable(
    ze_api_version_t version,
    zes_global_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnInit, L0::zesInit, version, ZE_API_VERSION_1_5);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetDriverProcAddrTable(
    ze_api_version_t version,
    zes_driver_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnEventListen, L0::zesDriverEventListen, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEventListenEx, L0::zesDriverEventListenEx, version, ZE_API_VERSION_1_1);
    fillDdiEntry(pDdiTable->pfnGet, L0::zesDriverGet, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetExtensionProperties, L0::zesDriverGetExtensionProperties, version, ZE_API_VERSION_1_8);
    fillDdiEntry(pDdiTable->pfnGetExtensionFunctionAddress, L0::zesDriverGetExtensionFunctionAddress, version, ZE_API_VERSION_1_8);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetDiagnosticsProcAddrTable(
    ze_api_version_t version,
    zes_diagnostics_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesDiagnosticsGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetTests, L0::zesDiagnosticsGetTests, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnRunTests, L0::zesDiagnosticsRunTests, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetEngineProcAddrTable(
    ze_api_version_t version,
    zes_engine_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesEngineGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetActivity, L0::zesEngineGetActivity, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetActivityExt, L0::zesEngineGetActivityExt, version, ZE_API_VERSION_1_7);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFabricPortProcAddrTable(
    ze_api_version_t version,
    zes_fabric_port_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesFabricPortGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetLinkType, L0::zesFabricPortGetLinkType, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetConfig, L0::zesFabricPortGetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetConfig, L0::zesFabricPortSetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::zesFabricPortGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetThroughput, L0::zesFabricPortGetThroughput, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetFabricErrorCounters, L0::zesFabricPortGetFabricErrorCounters, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnGetMultiPortThroughput, L0::zesFabricPortGetMultiPortThroughput, version, ZE_API_VERSION_1_7);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFanProcAddrTable(
    ze_api_version_t version,
    zes_fan_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesFanGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetConfig, L0::zesFanGetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetDefaultMode, L0::zesFanSetDefaultMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetFixedSpeedMode, L0::zesFanSetFixedSpeedMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetSpeedTableMode, L0::zesFanSetSpeedTableMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::zesFanGetState, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFirmwareProcAddrTable(
    ze_api_version_t version,
    zes_firmware_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesFirmwareGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnFlash, L0::zesFirmwareFlash, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetFlashProgress, L0::zesFirmwareGetFlashProgress, version, ZE_API_VERSION_1_8);
    fillDdiEntry(pDdiTable->pfnGetConsoleLogs, L0::zesFirmwareGetConsoleLogs, version, ZE_API_VERSION_1_9);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFrequencyProcAddrTable(
    ze_api_version_t version,
    zes_frequency_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesFrequencyGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetAvailableClocks, L0::zesFrequencyGetAvailableClocks, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetRange, L0::zesFrequencyGetRange, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetRange, L0::zesFrequencySetRange, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::zesFrequencyGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetThrottleTime, L0::zesFrequencyGetThrottleTime, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcGetCapabilities, L0::zesFrequencyOcGetCapabilities, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcGetFrequencyTarget, L0::zesFrequencyOcGetFrequencyTarget, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcSetFrequencyTarget, L0::zesFrequencyOcSetFrequencyTarget, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcGetVoltageTarget, L0::zesFrequencyOcGetVoltageTarget, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcSetVoltageTarget, L0::zesFrequencyOcSetVoltageTarget, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcSetMode, L0::zesFrequencyOcSetMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcGetMode, L0::zesFrequencyOcGetMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcGetIccMax, L0::zesFrequencyOcGetIccMax, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcSetIccMax, L0::zesFrequencyOcSetIccMax, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcGetTjMax, L0::zesFrequencyOcGetTjMax, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcSetTjMax, L0::zesFrequencyOcSetTjMax, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetLedProcAddrTable(
    ze_api_version_t version,
    zes_led_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesLedGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::zesLedGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetState, L0::zesLedSetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetColor, L0::zesLedSetColor, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetMemoryProcAddrTable(
    ze_api_version_t version,
    zes_memory_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesMemoryGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::zesMemoryGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetBandwidth, L0::zesMemoryGetBandwidth, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetPerformanceFactorProcAddrTable(
    ze_api_version_t version,
    zes_performance_factor_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesPerformanceFactorGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetConfig, L0::zesPerformanceFactorGetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetConfig, L0::zesPerformanceFactorSetConfig, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetPowerProcAddrTable(
    ze_api_version_t version,
    zes_power_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesPowerGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetEnergyCounter, L0::zesPowerGetEnergyCounter, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetLimits, L0::zesPowerGetLimits, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetLimits, L0::zesPowerSetLimits, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetEnergyThreshold, L0::zesPowerGetEnergyThreshold, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetEnergyThreshold, L0::zesPowerSetEnergyThreshold, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetLimitsExt, L0::zesPowerGetLimitsExt, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnSetLimitsExt, L0::zesPowerSetLimitsExt, version, ZE_API_VERSION_1_4);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetPsuProcAddrTable(
    ze_api_version_t version,
    zes_psu_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesPsuGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::zesPsuGetState, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetRasProcAddrTable(
    ze_api_version_t version,
    zes_ras_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesRasGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetConfig, L0::zesRasGetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetConfig, L0::zesRasSetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::zesRasGetState, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetRasExpProcAddrTable(
    ze_api_version_t version,
    zes_ras_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetStateExp, L0::zesRasGetStateExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnClearStateExp, L0::zesRasClearStateExp, version, ZE_API_VERSION_1_7);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL zesGetDriverExpProcAddrTable(
    ze_api_version_t version, zes_driver_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetDeviceByUuidExp, L0::zesDriverGetDeviceByUuidExp, version, ZE_API_VERSION_1_9);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL zesGetDeviceExpProcAddrTable(
    ze_api_version_t version,
    zes_device_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetSubDevicePropertiesExp, L0::zesDeviceGetSubDevicePropertiesExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnEnumActiveVFExp, L0::zesDeviceEnumActiveVFExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnEnumEnabledVFExp, L0::zesDeviceEnumEnabledVFExp, version, ZE_API_VERSION_1_11);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetVFManagementExpProcAddrTable(
    ze_api_version_t version,
    zes_vf_management_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetVFPropertiesExp, L0::zesVFManagementGetVFPropertiesExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetVFMemoryUtilizationExp, L0::zesVFManagementGetVFMemoryUtilizationExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetVFEngineUtilizationExp, L0::zesVFManagementGetVFEngineUtilizationExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnSetVFTelemetryModeExp, L0::zesVFManagementSetVFTelemetryModeExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnSetVFTelemetrySamplingIntervalExp, L0::zesVFManagementSetVFTelemetrySamplingIntervalExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetVFCapabilitiesExp, L0::zesVFManagementGetVFCapabilitiesExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnGetVFMemoryUtilizationExp2, L0::zesVFManagementGetVFMemoryUtilizationExp2, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnGetVFEngineUtilizationExp2, L0::zesVFManagementGetVFEngineUtilizationExp2, version, ZE_API_VERSION_1_11);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL zesGetFirmwareExpProcAddrTable(
    ze_api_version_t version,
    zes_firmware_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetSecurityVersionExp, L0::zesFirmwareGetSecurityVersionExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnSetSecurityVersionExp, L0::zesFirmwareSetSecurityVersionExp, version, ZE_API_VERSION_1_9);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetSchedulerProcAddrTable(
    ze_api_version_t version,
    zes_scheduler_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesSchedulerGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetCurrentMode, L0::zesSchedulerGetCurrentMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetTimeoutModeProperties, L0::zesSchedulerGetTimeoutModeProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetTimesliceModeProperties, L0::zesSchedulerGetTimesliceModeProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetTimeoutMode, L0::zesSchedulerSetTimeoutMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetTimesliceMode, L0::zesSchedulerSetTimesliceMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetExclusiveMode, L0::zesSchedulerSetExclusiveMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetComputeUnitDebugMode, L0::zesSchedulerSetComputeUnitDebugMode, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetStandbyProcAddrTable(
    ze_api_version_t version,
    zes_standby_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesStandbyGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetMode, L0::zesStandbyGetMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetMode, L0::zesStandbySetMode, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetTemperatureProcAddrTable(
    ze_api_version_t version,
    zes_temperature_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zesTemperatureGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetConfig, L0::zesTemperatureGetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetConfig, L0::zesTemperatureSetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::zesTemperatureGetState, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetOverclockProcAddrTable(
    ze_api_version_t version,           ///< [in] API version requested
    zes_overclock_dditable_t *pDdiTable ///< [in,out] pointer to table of DDI function pointers
) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetDomainProperties, L0::zesOverclockGetDomainProperties, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetDomainVFProperties, L0::zesOverclockGetDomainVFProperties, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetDomainControlProperties, L0::zesOverclockGetDomainControlProperties, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetControlCurrentValue, L0::zesOverclockGetControlCurrentValue, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetControlPendingValue, L0::zesOverclockGetControlPendingValue, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnSetControlUserValue, L0::zesOverclockSetControlUserValue, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetControlState, L0::zesOverclockGetControlState, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetVFPointValues, L0::zesOverclockGetVFPointValues, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnSetVFPointValues, L0::zesOverclockSetVFPointValues, version, ZE_API_VERSION_1_5);

    return result;
}
