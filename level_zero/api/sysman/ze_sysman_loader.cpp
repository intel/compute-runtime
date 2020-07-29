/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/source/inc/ze_intel_gpu.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zes_api.h>
#include <level_zero/zes_ddi.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

#include "ze_ddi_tables.h"

extern ze_gpu_driver_dditable_t driver_ddiTable;

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetDeviceProcAddrTable(
    ze_api_version_t version,
    zes_device_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = (zes_pfnDeviceGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceGetProperties");
    pDdiTable->pfnGetState = (zes_pfnDeviceGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceGetState");
    pDdiTable->pfnReset = (zes_pfnDeviceReset_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceReset");
    pDdiTable->pfnProcessesGetState = (zes_pfnDeviceProcessesGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceProcessesGetState");
    pDdiTable->pfnPciGetProperties = (zes_pfnDevicePciGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDevicePciGetProperties");
    pDdiTable->pfnPciGetState = (zes_pfnDevicePciGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDevicePciGetState");
    pDdiTable->pfnPciGetBars = (zes_pfnDevicePciGetBars_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDevicePciGetBars");
    pDdiTable->pfnPciGetStats = (zes_pfnDevicePciGetStats_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDevicePciGetStats");
    pDdiTable->pfnEnumDiagnosticTestSuites = (zes_pfnDeviceEnumDiagnosticTestSuites_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumDiagnosticTestSuites");
    pDdiTable->pfnEnumEngineGroups = (zes_pfnDeviceEnumEngineGroups_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumEngineGroups");
    pDdiTable->pfnEventRegister = (zes_pfnDeviceEventRegister_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEventRegister");
    pDdiTable->pfnEnumFabricPorts = (zes_pfnDeviceEnumFabricPorts_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumFabricPorts");
    pDdiTable->pfnEnumFans = (zes_pfnDeviceEnumFans_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumFans");
    pDdiTable->pfnEnumFirmwares = (zes_pfnDeviceEnumFirmwares_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumFirmwares");
    pDdiTable->pfnEnumFrequencyDomains = (zes_pfnDeviceEnumFrequencyDomains_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumFrequencyDomains");
    pDdiTable->pfnEnumLeds = (zes_pfnDeviceEnumLeds_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumLeds");
    pDdiTable->pfnEnumMemoryModules = (zes_pfnDeviceEnumMemoryModules_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumMemoryModules");
    pDdiTable->pfnEnumPerformanceFactorDomains = (zes_pfnDeviceEnumPerformanceFactorDomains_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumPerformanceFactorDomains");
    pDdiTable->pfnEnumPowerDomains = (zes_pfnDeviceEnumPowerDomains_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumPowerDomains");
    pDdiTable->pfnEnumPsus = (zes_pfnDeviceEnumPsus_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumPsus");
    pDdiTable->pfnEnumRasErrorSets = (zes_pfnDeviceEnumRasErrorSets_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumRasErrorSets");
    pDdiTable->pfnEnumSchedulers = (zes_pfnDeviceEnumSchedulers_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumSchedulers");
    pDdiTable->pfnEnumStandbyDomains = (zes_pfnDeviceEnumStandbyDomains_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumStandbyDomains");
    pDdiTable->pfnEnumTemperatureSensors = (zes_pfnDeviceEnumTemperatureSensors_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDeviceEnumTemperatureSensors");
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetDriverProcAddrTable(
    ze_api_version_t version,
    zes_driver_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnEventListen = (zes_pfnDriverEventListen_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDriverEventListen");
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetDiagnosticsProcAddrTable(
    ze_api_version_t version,
    zes_diagnostics_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zes_pfnDiagnosticsGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDiagnosticsGetProperties");
    pDdiTable->pfnGetTests = (zes_pfnDiagnosticsGetTests_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDiagnosticsGetTests");
    pDdiTable->pfnRunTests = (zes_pfnDiagnosticsRunTests_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesDiagnosticsRunTests");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetEngineProcAddrTable(
    ze_api_version_t version,
    zes_engine_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zes_pfnEngineGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesEngineGetProperties");
    pDdiTable->pfnGetActivity = (zes_pfnEngineGetActivity_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesEngineGetActivity");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFabricPortProcAddrTable(
    ze_api_version_t version,
    zes_fabric_port_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = (zes_pfnFabricPortGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFabricPortGetProperties");
    pDdiTable->pfnGetLinkType = (zes_pfnFabricPortGetLinkType_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFabricPortGetLinkType");
    pDdiTable->pfnGetConfig = (zes_pfnFabricPortGetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFabricPortGetConfig");
    pDdiTable->pfnSetConfig = (zes_pfnFabricPortSetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFabricPortSetConfig");
    pDdiTable->pfnGetState = (zes_pfnFabricPortGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFabricPortGetState");
    pDdiTable->pfnGetThroughput = (zes_pfnFabricPortGetThroughput_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFabricPortGetThroughput");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFanProcAddrTable(
    ze_api_version_t version,
    zes_fan_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = (zes_pfnFanGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFanGetProperties");
    pDdiTable->pfnGetConfig = (zes_pfnFanGetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFanGetConfig");
    pDdiTable->pfnSetDefaultMode = (zes_pfnFanSetDefaultMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFanSetDefaultMode");
    pDdiTable->pfnSetFixedSpeedMode = (zes_pfnFanSetFixedSpeedMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFanSetFixedSpeedMode");
    pDdiTable->pfnSetSpeedTableMode = (zes_pfnFanSetSpeedTableMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFanSetSpeedTableMode");
    pDdiTable->pfnGetState = (zes_pfnFanGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFanGetState");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFirmwareProcAddrTable(
    ze_api_version_t version,
    zes_firmware_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zes_pfnFirmwareGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFirmwareGetProperties");
    pDdiTable->pfnFlash = (zes_pfnFirmwareFlash_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFirmwareFlash");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFrequencyProcAddrTable(
    ze_api_version_t version,
    zes_frequency_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zes_pfnFrequencyGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyGetProperties");
    pDdiTable->pfnGetAvailableClocks = (zes_pfnFrequencyGetAvailableClocks_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyGetAvailableClocks");
    pDdiTable->pfnGetRange = (zes_pfnFrequencyGetRange_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyGetRange");
    pDdiTable->pfnSetRange = (zes_pfnFrequencySetRange_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencySetRange");
    pDdiTable->pfnGetState = (zes_pfnFrequencyGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyGetState");
    pDdiTable->pfnGetThrottleTime = (zes_pfnFrequencyGetThrottleTime_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyGetThrottleTime");
    pDdiTable->pfnOcGetCapabilities = (zes_pfnFrequencyOcGetCapabilities_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyOcGetCapabilities");
    pDdiTable->pfnOcGetFrequencyTarget = (zes_pfnFrequencyOcGetFrequencyTarget_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyOcGetFrequencyTarget");
    pDdiTable->pfnOcSetFrequencyTarget = (zes_pfnFrequencyOcSetFrequencyTarget_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyOcSetFrequencyTarget");
    pDdiTable->pfnOcGetVoltageTarget = (zes_pfnFrequencyOcGetVoltageTarget_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyOcGetVoltageTarget");
    pDdiTable->pfnOcSetVoltageTarget = (zes_pfnFrequencyOcSetVoltageTarget_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyOcSetVoltageTarget");
    pDdiTable->pfnOcSetMode = (zes_pfnFrequencyOcSetMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyOcSetMode");
    pDdiTable->pfnOcGetMode = (zes_pfnFrequencyOcGetMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyOcGetMode");
    pDdiTable->pfnOcGetIccMax = (zes_pfnFrequencyOcGetIccMax_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyOcGetIccMax");
    pDdiTable->pfnOcSetIccMax = (zes_pfnFrequencyOcSetIccMax_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyOcSetIccMax");
    pDdiTable->pfnOcGetTjMax = (zes_pfnFrequencyOcGetTjMax_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyOcGetTjMax");
    pDdiTable->pfnOcSetTjMax = (zes_pfnFrequencyOcSetTjMax_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesFrequencyOcSetTjMax");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetLedProcAddrTable(
    ze_api_version_t version,
    zes_led_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zes_pfnLedGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesLedGetProperties");
    pDdiTable->pfnGetState = (zes_pfnLedGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesLedGetState");
    pDdiTable->pfnSetState = (zes_pfnLedSetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesLedSetState");
    pDdiTable->pfnSetColor = (zes_pfnLedSetColor_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesLedSetColor");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetMemoryProcAddrTable(
    ze_api_version_t version,
    zes_memory_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zes_pfnMemoryGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesMemoryGetProperties");
    pDdiTable->pfnGetState = (zes_pfnMemoryGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesMemoryGetState");
    pDdiTable->pfnGetBandwidth = (zes_pfnMemoryGetBandwidth_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesMemoryGetBandwidth");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetPerformanceFactorProcAddrTable(
    ze_api_version_t version,
    zes_performance_factor_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zes_pfnPerformanceFactorGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesPerformanceFactorGetProperties");
    pDdiTable->pfnGetConfig = (zes_pfnPerformanceFactorGetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesPerformanceFactorGetConfig");
    pDdiTable->pfnSetConfig = (zes_pfnPerformanceFactorSetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesPerformanceFactorSetConfig");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetPowerProcAddrTable(
    ze_api_version_t version,
    zes_power_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zes_pfnPowerGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesPowerGetProperties");
    pDdiTable->pfnGetEnergyCounter = (zes_pfnPowerGetEnergyCounter_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesPowerGetEnergyCounter");
    pDdiTable->pfnGetLimits = (zes_pfnPowerGetLimits_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesPowerGetLimits");
    pDdiTable->pfnSetLimits = (zes_pfnPowerSetLimits_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesPowerSetLimits");
    pDdiTable->pfnGetEnergyThreshold = (zes_pfnPowerGetEnergyThreshold_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesPowerGetEnergyThreshold");
    pDdiTable->pfnSetEnergyThreshold = (zes_pfnPowerSetEnergyThreshold_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesPowerSetEnergyThreshold");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetPsuProcAddrTable(
    ze_api_version_t version,
    zes_psu_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zes_pfnPsuGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesPsuGetProperties");
    pDdiTable->pfnGetState = (zes_pfnPsuGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesPsuGetState");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetRasProcAddrTable(
    ze_api_version_t version,
    zes_ras_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zes_pfnRasGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesRasGetProperties");
    pDdiTable->pfnGetConfig = (zes_pfnRasGetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesRasGetConfig");
    pDdiTable->pfnSetConfig = (zes_pfnRasSetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesRasSetConfig");
    pDdiTable->pfnGetState = (zes_pfnRasGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesRasGetState");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetSchedulerProcAddrTable(
    ze_api_version_t version,
    zes_scheduler_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zes_pfnSchedulerGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesSchedulerGetProperties");
    pDdiTable->pfnGetCurrentMode = (zes_pfnSchedulerGetCurrentMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesSchedulerGetCurrentMode");
    pDdiTable->pfnGetTimeoutModeProperties = (zes_pfnSchedulerGetTimeoutModeProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesSchedulerGetTimeoutModeProperties");
    pDdiTable->pfnGetTimesliceModeProperties = (zes_pfnSchedulerGetTimesliceModeProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesSchedulerGetTimesliceModeProperties");
    pDdiTable->pfnSetTimeoutMode = (zes_pfnSchedulerSetTimeoutMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesSchedulerSetTimeoutMode");
    pDdiTable->pfnSetTimesliceMode = (zes_pfnSchedulerSetTimesliceMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesSchedulerSetTimesliceMode");
    pDdiTable->pfnSetExclusiveMode = (zes_pfnSchedulerSetExclusiveMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesSchedulerSetExclusiveMode");
    pDdiTable->pfnSetComputeUnitDebugMode = (zes_pfnSchedulerSetComputeUnitDebugMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesSchedulerSetComputeUnitDebugMode");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetStandbyProcAddrTable(
    ze_api_version_t version,
    zes_standby_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zes_pfnStandbyGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesStandbyGetProperties");
    pDdiTable->pfnGetMode = (zes_pfnStandbyGetMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesStandbyGetMode");
    pDdiTable->pfnSetMode = (zes_pfnStandbySetMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesStandbySetMode");

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetTemperatureProcAddrTable(
    ze_api_version_t version,
    zes_temperature_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zes_pfnTemperatureGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesTemperatureGetProperties");
    pDdiTable->pfnGetConfig = (zes_pfnTemperatureGetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesTemperatureGetConfig");
    pDdiTable->pfnSetConfig = (zes_pfnTemperatureSetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesTemperatureSetConfig");
    pDdiTable->pfnGetState = (zes_pfnTemperatureGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zesTemperatureGetState");

    return result;
}
