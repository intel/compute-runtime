/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/source/inc/ze_intel_gpu.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

#include "ze_ddi_tables.h"

extern "C" {

extern ze_gpu_driver_dditable_t driver_ddiTable;

__zedllexport ze_result_t __zecall
zetGetGlobalProcAddrTable(
    ze_api_version_t version,
    zet_global_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnInit = (zet_pfnInit_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetInit");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetDeviceProcAddrTable(
    ze_api_version_t version,
    zet_device_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnActivateMetricGroups = (zet_pfnDeviceActivateMetricGroups_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetDeviceActivateMetricGroups");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetCommandListProcAddrTable(
    ze_api_version_t version,
    zet_command_list_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnAppendMetricTracerMarker = (zet_pfnCommandListAppendMetricTracerMarker_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetCommandListAppendMetricTracerMarker");
    pDdiTable->pfnAppendMetricQueryBegin = (zet_pfnCommandListAppendMetricQueryBegin_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetCommandListAppendMetricQueryBegin");
    pDdiTable->pfnAppendMetricQueryEnd = (zet_pfnCommandListAppendMetricQueryEnd_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetCommandListAppendMetricQueryEnd");
    pDdiTable->pfnAppendMetricMemoryBarrier = (zet_pfnCommandListAppendMetricMemoryBarrier_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetCommandListAppendMetricMemoryBarrier");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetModuleProcAddrTable(
    ze_api_version_t version,
    zet_module_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetDebugInfo = (zet_pfnModuleGetDebugInfo_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetModuleGetDebugInfo");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetKernelProcAddrTable(
    ze_api_version_t version,
    zet_kernel_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProfileInfo = (zet_pfnKernelGetProfileInfo_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetKernelGetProfileInfo");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetMetricGroupProcAddrTable(
    ze_api_version_t version,
    zet_metric_group_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGet = (zet_pfnMetricGroupGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricGroupGet");
    pDdiTable->pfnGetProperties = (zet_pfnMetricGroupGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricGroupGetProperties");
    pDdiTable->pfnCalculateMetricValues = (zet_pfnMetricGroupCalculateMetricValues_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricGroupCalculateMetricValues");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetMetricProcAddrTable(
    ze_api_version_t version,
    zet_metric_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGet = (zet_pfnMetricGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricGet");
    pDdiTable->pfnGetProperties = (zet_pfnMetricGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricGetProperties");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetMetricTracerProcAddrTable(
    ze_api_version_t version,
    zet_metric_tracer_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnOpen = (zet_pfnMetricTracerOpen_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricTracerOpen");
    pDdiTable->pfnClose = (zet_pfnMetricTracerClose_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricTracerClose");
    pDdiTable->pfnReadData = (zet_pfnMetricTracerReadData_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricTracerReadData");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetMetricQueryPoolProcAddrTable(
    ze_api_version_t version,
    zet_metric_query_pool_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (zet_pfnMetricQueryPoolCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricQueryPoolCreate");
    pDdiTable->pfnDestroy = (zet_pfnMetricQueryPoolDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricQueryPoolDestroy");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetMetricQueryProcAddrTable(
    ze_api_version_t version,
    zet_metric_query_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (zet_pfnMetricQueryCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricQueryCreate");
    pDdiTable->pfnDestroy = (zet_pfnMetricQueryDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricQueryDestroy");
    pDdiTable->pfnReset = (zet_pfnMetricQueryReset_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricQueryReset");
    pDdiTable->pfnGetData = (zet_pfnMetricQueryGetData_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetMetricQueryGetData");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetTracerProcAddrTable(
    ze_api_version_t version,
    zet_tracer_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (zet_pfnTracerCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetTracerCreate");
    pDdiTable->pfnDestroy = (zet_pfnTracerDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetTracerDestroy");
    pDdiTable->pfnSetPrologues = (zet_pfnTracerSetPrologues_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetTracerSetPrologues");
    pDdiTable->pfnSetEpilogues = (zet_pfnTracerSetEpilogues_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetTracerSetEpilogues");
    pDdiTable->pfnSetEnabled = (zet_pfnTracerSetEnabled_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetTracerSetEnabled");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanProcAddrTable(
    ze_api_version_t version,
    zet_sysman_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGet = (zet_pfnSysmanGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanGet");
    pDdiTable->pfnDeviceGetProperties = (zet_pfnSysmanDeviceGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanDeviceGetProperties");
    pDdiTable->pfnSchedulerGetSupportedModes = (zet_pfnSysmanSchedulerGetSupportedModes_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanSchedulerGetSupportedModes");
    pDdiTable->pfnSchedulerGetCurrentMode = (zet_pfnSysmanSchedulerGetCurrentMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanSchedulerGetCurrentMode");
    pDdiTable->pfnSchedulerGetTimeoutModeProperties = (zet_pfnSysmanSchedulerGetTimeoutModeProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanSchedulerGetTimeoutModeProperties");
    pDdiTable->pfnSchedulerGetTimesliceModeProperties = (zet_pfnSysmanSchedulerGetTimesliceModeProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanSchedulerGetTimesliceModeProperties");
    pDdiTable->pfnSchedulerSetTimeoutMode = (zet_pfnSysmanSchedulerSetTimeoutMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanSchedulerSetTimeoutMode");
    pDdiTable->pfnSchedulerSetTimesliceMode = (zet_pfnSysmanSchedulerSetTimesliceMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanSchedulerSetTimesliceMode");
    pDdiTable->pfnSchedulerSetExclusiveMode = (zet_pfnSysmanSchedulerSetExclusiveMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanSchedulerSetExclusiveMode");
    pDdiTable->pfnSchedulerSetComputeUnitDebugMode = (zet_pfnSysmanSchedulerSetComputeUnitDebugMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanSchedulerSetComputeUnitDebugMode");
    pDdiTable->pfnPerformanceProfileGetSupported = (zet_pfnSysmanPerformanceProfileGetSupported_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPerformanceProfileGetSupported");
    pDdiTable->pfnPerformanceProfileGet = (zet_pfnSysmanPerformanceProfileGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPerformanceProfileGet");
    pDdiTable->pfnPerformanceProfileSet = (zet_pfnSysmanPerformanceProfileSet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPerformanceProfileSet");
    pDdiTable->pfnProcessesGetState = (zet_pfnSysmanProcessesGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanProcessesGetState");
    pDdiTable->pfnDeviceReset = (zet_pfnSysmanDeviceReset_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanDeviceReset");
    pDdiTable->pfnDeviceGetRepairStatus = (zet_pfnSysmanDeviceGetRepairStatus_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanDeviceGetRepairStatus");
    pDdiTable->pfnPciGetProperties = (zet_pfnSysmanPciGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPciGetProperties");
    pDdiTable->pfnPciGetState = (zet_pfnSysmanPciGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPciGetState");
    pDdiTable->pfnPciGetBars = (zet_pfnSysmanPciGetBars_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPciGetBars");
    pDdiTable->pfnPciGetStats = (zet_pfnSysmanPciGetStats_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPciGetStats");
    pDdiTable->pfnPowerGet = (zet_pfnSysmanPowerGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPowerGet");
    pDdiTable->pfnFrequencyGet = (zet_pfnSysmanFrequencyGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencyGet");
    pDdiTable->pfnEngineGet = (zet_pfnSysmanEngineGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanEngineGet");
    pDdiTable->pfnStandbyGet = (zet_pfnSysmanStandbyGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanStandbyGet");
    pDdiTable->pfnFirmwareGet = (zet_pfnSysmanFirmwareGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFirmwareGet");
    pDdiTable->pfnMemoryGet = (zet_pfnSysmanMemoryGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanMemoryGet");
    pDdiTable->pfnFabricPortGet = (zet_pfnSysmanFabricPortGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFabricPortGet");
    pDdiTable->pfnTemperatureGet = (zet_pfnSysmanTemperatureGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanTemperatureGet");
    pDdiTable->pfnPsuGet = (zet_pfnSysmanPsuGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPsuGet");
    pDdiTable->pfnFanGet = (zet_pfnSysmanFanGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFanGet");
    pDdiTable->pfnLedGet = (zet_pfnSysmanLedGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanLedGet");
    pDdiTable->pfnRasGet = (zet_pfnSysmanRasGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanRasGet");
    pDdiTable->pfnEventGet = (zet_pfnSysmanEventGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanEventGet");
    pDdiTable->pfnDiagnosticsGet = (zet_pfnSysmanDiagnosticsGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanDiagnosticsGet");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanPowerProcAddrTable(
    ze_api_version_t version,
    zet_sysman_power_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zet_pfnSysmanPowerGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPowerGetProperties");
    pDdiTable->pfnGetEnergyCounter = (zet_pfnSysmanPowerGetEnergyCounter_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPowerGetEnergyCounter");
    pDdiTable->pfnGetLimits = (zet_pfnSysmanPowerGetLimits_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPowerGetLimits");
    pDdiTable->pfnSetLimits = (zet_pfnSysmanPowerSetLimits_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPowerSetLimits");
    pDdiTable->pfnGetEnergyThreshold = (zet_pfnSysmanPowerGetEnergyThreshold_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPowerGetEnergyThreshold");
    pDdiTable->pfnSetEnergyThreshold = (zet_pfnSysmanPowerSetEnergyThreshold_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPowerSetEnergyThreshold");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanFrequencyProcAddrTable(
    ze_api_version_t version,
    zet_sysman_frequency_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zet_pfnSysmanFrequencyGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencyGetProperties");
    pDdiTable->pfnGetAvailableClocks = (zet_pfnSysmanFrequencyGetAvailableClocks_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencyGetAvailableClocks");
    pDdiTable->pfnGetRange = (zet_pfnSysmanFrequencyGetRange_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencyGetRange");
    pDdiTable->pfnSetRange = (zet_pfnSysmanFrequencySetRange_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencySetRange");
    pDdiTable->pfnGetState = (zet_pfnSysmanFrequencyGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencyGetState");
    pDdiTable->pfnGetThrottleTime = (zet_pfnSysmanFrequencyGetThrottleTime_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencyGetThrottleTime");
    pDdiTable->pfnOcGetCapabilities = (zet_pfnSysmanFrequencyOcGetCapabilities_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencyOcGetCapabilities");
    pDdiTable->pfnOcGetConfig = (zet_pfnSysmanFrequencyOcGetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencyOcGetConfig");
    pDdiTable->pfnOcSetConfig = (zet_pfnSysmanFrequencyOcSetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencyOcSetConfig");
    pDdiTable->pfnOcGetIccMax = (zet_pfnSysmanFrequencyOcGetIccMax_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencyOcGetIccMax");
    pDdiTable->pfnOcSetIccMax = (zet_pfnSysmanFrequencyOcSetIccMax_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencyOcSetIccMax");
    pDdiTable->pfnOcGetTjMax = (zet_pfnSysmanFrequencyOcGetTjMax_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencyOcGetTjMax");
    pDdiTable->pfnOcSetTjMax = (zet_pfnSysmanFrequencyOcSetTjMax_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFrequencyOcSetTjMax");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanEngineProcAddrTable(
    ze_api_version_t version,
    zet_sysman_engine_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zet_pfnSysmanEngineGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanEngineGetProperties");
    pDdiTable->pfnGetActivity = (zet_pfnSysmanEngineGetActivity_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanEngineGetActivity");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanStandbyProcAddrTable(
    ze_api_version_t version,
    zet_sysman_standby_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zet_pfnSysmanStandbyGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanStandbyGetProperties");
    pDdiTable->pfnGetMode = (zet_pfnSysmanStandbyGetMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanStandbyGetMode");
    pDdiTable->pfnSetMode = (zet_pfnSysmanStandbySetMode_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanStandbySetMode");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanFirmwareProcAddrTable(
    ze_api_version_t version,
    zet_sysman_firmware_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zet_pfnSysmanFirmwareGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFirmwareGetProperties");
    pDdiTable->pfnGetChecksum = (zet_pfnSysmanFirmwareGetChecksum_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFirmwareGetChecksum");
    pDdiTable->pfnFlash = (zet_pfnSysmanFirmwareFlash_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFirmwareFlash");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanMemoryProcAddrTable(
    ze_api_version_t version,
    zet_sysman_memory_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zet_pfnSysmanMemoryGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanMemoryGetProperties");
    pDdiTable->pfnGetState = (zet_pfnSysmanMemoryGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanMemoryGetState");
    pDdiTable->pfnGetBandwidth = (zet_pfnSysmanMemoryGetBandwidth_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanMemoryGetBandwidth");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanFabricPortProcAddrTable(
    ze_api_version_t version,
    zet_sysman_fabric_port_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zet_pfnSysmanFabricPortGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFabricPortGetProperties");
    pDdiTable->pfnGetLinkType = (zet_pfnSysmanFabricPortGetLinkType_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFabricPortGetLinkType");
    pDdiTable->pfnGetConfig = (zet_pfnSysmanFabricPortGetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFabricPortGetConfig");
    pDdiTable->pfnSetConfig = (zet_pfnSysmanFabricPortSetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFabricPortSetConfig");
    pDdiTable->pfnGetState = (zet_pfnSysmanFabricPortGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFabricPortGetState");
    pDdiTable->pfnGetThroughput = (zet_pfnSysmanFabricPortGetThroughput_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFabricPortGetThroughput");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanTemperatureProcAddrTable(
    ze_api_version_t version,
    zet_sysman_temperature_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zet_pfnSysmanTemperatureGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanTemperatureGetProperties");
    pDdiTable->pfnGetConfig = (zet_pfnSysmanTemperatureGetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanTemperatureGetConfig");
    pDdiTable->pfnSetConfig = (zet_pfnSysmanTemperatureSetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanTemperatureSetConfig");
    pDdiTable->pfnGetState = (zet_pfnSysmanTemperatureGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanTemperatureGetState");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanPsuProcAddrTable(
    ze_api_version_t version,
    zet_sysman_psu_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zet_pfnSysmanPsuGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPsuGetProperties");
    pDdiTable->pfnGetState = (zet_pfnSysmanPsuGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanPsuGetState");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanFanProcAddrTable(
    ze_api_version_t version,
    zet_sysman_fan_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zet_pfnSysmanFanGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFanGetProperties");
    pDdiTable->pfnGetConfig = (zet_pfnSysmanFanGetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFanGetConfig");
    pDdiTable->pfnSetConfig = (zet_pfnSysmanFanSetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFanSetConfig");
    pDdiTable->pfnGetState = (zet_pfnSysmanFanGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanFanGetState");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanLedProcAddrTable(
    ze_api_version_t version,
    zet_sysman_led_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zet_pfnSysmanLedGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanLedGetProperties");
    pDdiTable->pfnGetState = (zet_pfnSysmanLedGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanLedGetState");
    pDdiTable->pfnSetState = (zet_pfnSysmanLedSetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanLedSetState");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanRasProcAddrTable(
    ze_api_version_t version,
    zet_sysman_ras_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zet_pfnSysmanRasGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanRasGetProperties");
    pDdiTable->pfnGetConfig = (zet_pfnSysmanRasGetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanRasGetConfig");
    pDdiTable->pfnSetConfig = (zet_pfnSysmanRasSetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanRasSetConfig");
    pDdiTable->pfnGetState = (zet_pfnSysmanRasGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanRasGetState");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanDiagnosticsProcAddrTable(
    ze_api_version_t version,
    zet_sysman_diagnostics_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (zet_pfnSysmanDiagnosticsGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanDiagnosticsGetProperties");
    pDdiTable->pfnGetTests = (zet_pfnSysmanDiagnosticsGetTests_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanDiagnosticsGetTests");
    pDdiTable->pfnRunTests = (zet_pfnSysmanDiagnosticsRunTests_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanDiagnosticsRunTests");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetSysmanEventProcAddrTable(
    ze_api_version_t version,
    zet_sysman_event_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetConfig = (zet_pfnSysmanEventGetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanEventGetConfig");
    pDdiTable->pfnSetConfig = (zet_pfnSysmanEventSetConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanEventSetConfig");
    pDdiTable->pfnGetState = (zet_pfnSysmanEventGetState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanEventGetState");
    pDdiTable->pfnListen = (zet_pfnSysmanEventListen_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetSysmanEventListen");
    return result;
}

__zedllexport ze_result_t __zecall
zetGetDebugProcAddrTable(
    ze_api_version_t version,
    zet_debug_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnAttach = (zet_pfnDebugAttach_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetDebugAttach");
    pDdiTable->pfnDetach = (zet_pfnDebugDetach_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetDebugDetach");
    pDdiTable->pfnGetNumThreads = (zet_pfnDebugGetNumThreads_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetDebugGetNumThreads");
    pDdiTable->pfnReadEvent = (zet_pfnDebugReadEvent_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetDebugReadEvent");
    pDdiTable->pfnInterrupt = (zet_pfnDebugInterrupt_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetDebugInterrupt");
    pDdiTable->pfnResume = (zet_pfnDebugResume_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetDebugResume");
    pDdiTable->pfnReadMemory = (zet_pfnDebugReadMemory_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetDebugReadMemory");
    pDdiTable->pfnWriteMemory = (zet_pfnDebugWriteMemory_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetDebugWriteMemory");
    pDdiTable->pfnReadState = (zet_pfnDebugReadState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetDebugReadState");
    pDdiTable->pfnWriteState = (zet_pfnDebugWriteState_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zetDebugWriteState");
    return result;
}
};
