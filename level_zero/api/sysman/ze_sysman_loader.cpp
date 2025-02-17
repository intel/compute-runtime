/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/ddi/ze_ddi_tables.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zes_api.h>
#include <level_zero/zes_ddi.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetDeviceProcAddrTable(
    ze_api_version_t version,
    zes_device_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanDevice.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::globalDriverDispatch.sysmanDevice.pfnGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReset, L0::globalDriverDispatch.sysmanDevice.pfnReset, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnProcessesGetState, L0::globalDriverDispatch.sysmanDevice.pfnProcessesGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnPciGetProperties, L0::globalDriverDispatch.sysmanDevice.pfnPciGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnPciGetState, L0::globalDriverDispatch.sysmanDevice.pfnPciGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnPciGetBars, L0::globalDriverDispatch.sysmanDevice.pfnPciGetBars, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnPciGetStats, L0::globalDriverDispatch.sysmanDevice.pfnPciGetStats, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumDiagnosticTestSuites, L0::globalDriverDispatch.sysmanDevice.pfnEnumDiagnosticTestSuites, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumEngineGroups, L0::globalDriverDispatch.sysmanDevice.pfnEnumEngineGroups, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEventRegister, L0::globalDriverDispatch.sysmanDevice.pfnEventRegister, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumFabricPorts, L0::globalDriverDispatch.sysmanDevice.pfnEnumFabricPorts, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumFans, L0::globalDriverDispatch.sysmanDevice.pfnEnumFans, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumFirmwares, L0::globalDriverDispatch.sysmanDevice.pfnEnumFirmwares, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumFrequencyDomains, L0::globalDriverDispatch.sysmanDevice.pfnEnumFrequencyDomains, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumLeds, L0::globalDriverDispatch.sysmanDevice.pfnEnumLeds, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumMemoryModules, L0::globalDriverDispatch.sysmanDevice.pfnEnumMemoryModules, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumPerformanceFactorDomains, L0::globalDriverDispatch.sysmanDevice.pfnEnumPerformanceFactorDomains, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumPowerDomains, L0::globalDriverDispatch.sysmanDevice.pfnEnumPowerDomains, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumPsus, L0::globalDriverDispatch.sysmanDevice.pfnEnumPsus, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumRasErrorSets, L0::globalDriverDispatch.sysmanDevice.pfnEnumRasErrorSets, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumSchedulers, L0::globalDriverDispatch.sysmanDevice.pfnEnumSchedulers, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumStandbyDomains, L0::globalDriverDispatch.sysmanDevice.pfnEnumStandbyDomains, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEnumTemperatureSensors, L0::globalDriverDispatch.sysmanDevice.pfnEnumTemperatureSensors, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetCardPowerDomain, L0::globalDriverDispatch.sysmanDevice.pfnGetCardPowerDomain, version, ZE_API_VERSION_1_3);
    fillDdiEntry(pDdiTable->pfnEccAvailable, L0::globalDriverDispatch.sysmanDevice.pfnEccAvailable, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnEccConfigurable, L0::globalDriverDispatch.sysmanDevice.pfnEccConfigurable, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGetEccState, L0::globalDriverDispatch.sysmanDevice.pfnGetEccState, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnSetEccState, L0::globalDriverDispatch.sysmanDevice.pfnSetEccState, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGet, L0::globalDriverDispatch.sysmanDevice.pfnGet, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnSetOverclockWaiver, L0::globalDriverDispatch.sysmanDevice.pfnSetOverclockWaiver, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetOverclockDomains, L0::globalDriverDispatch.sysmanDevice.pfnGetOverclockDomains, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetOverclockControls, L0::globalDriverDispatch.sysmanDevice.pfnGetOverclockControls, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnResetOverclockSettings, L0::globalDriverDispatch.sysmanDevice.pfnResetOverclockSettings, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnReadOverclockState, L0::globalDriverDispatch.sysmanDevice.pfnReadOverclockState, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnEnumOverclockDomains, L0::globalDriverDispatch.sysmanDevice.pfnEnumOverclockDomains, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnResetExt, L0::globalDriverDispatch.sysmanDevice.pfnResetExt, version, ZE_API_VERSION_1_7);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetGlobalProcAddrTable(
    ze_api_version_t version,
    zes_global_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnInit, L0::globalDriverDispatch.sysmanGlobal.pfnInit, version, ZE_API_VERSION_1_5);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetDriverProcAddrTable(
    ze_api_version_t version,
    zes_driver_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnEventListen, L0::globalDriverDispatch.sysmanDriver.pfnEventListen, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEventListenEx, L0::globalDriverDispatch.sysmanDriver.pfnEventListenEx, version, ZE_API_VERSION_1_1);
    fillDdiEntry(pDdiTable->pfnGet, L0::globalDriverDispatch.sysmanDriver.pfnGet, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetExtensionProperties, L0::globalDriverDispatch.sysmanDriver.pfnGetExtensionProperties, version, ZE_API_VERSION_1_8);
    fillDdiEntry(pDdiTable->pfnGetExtensionFunctionAddress, L0::globalDriverDispatch.sysmanDriver.pfnGetExtensionFunctionAddress, version, ZE_API_VERSION_1_8);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetDiagnosticsProcAddrTable(
    ze_api_version_t version,
    zes_diagnostics_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanDiagnostics.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetTests, L0::globalDriverDispatch.sysmanDiagnostics.pfnGetTests, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnRunTests, L0::globalDriverDispatch.sysmanDiagnostics.pfnRunTests, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetEngineProcAddrTable(
    ze_api_version_t version,
    zes_engine_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanEngine.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetActivity, L0::globalDriverDispatch.sysmanEngine.pfnGetActivity, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetActivityExt, L0::globalDriverDispatch.sysmanEngine.pfnGetActivityExt, version, ZE_API_VERSION_1_7);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFabricPortProcAddrTable(
    ze_api_version_t version,
    zes_fabric_port_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanFabricPort.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetLinkType, L0::globalDriverDispatch.sysmanFabricPort.pfnGetLinkType, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetConfig, L0::globalDriverDispatch.sysmanFabricPort.pfnGetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetConfig, L0::globalDriverDispatch.sysmanFabricPort.pfnSetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::globalDriverDispatch.sysmanFabricPort.pfnGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetThroughput, L0::globalDriverDispatch.sysmanFabricPort.pfnGetThroughput, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetFabricErrorCounters, L0::globalDriverDispatch.sysmanFabricPort.pfnGetFabricErrorCounters, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnGetMultiPortThroughput, L0::globalDriverDispatch.sysmanFabricPort.pfnGetMultiPortThroughput, version, ZE_API_VERSION_1_7);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFanProcAddrTable(
    ze_api_version_t version,
    zes_fan_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanFan.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetConfig, L0::globalDriverDispatch.sysmanFan.pfnGetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetDefaultMode, L0::globalDriverDispatch.sysmanFan.pfnSetDefaultMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetFixedSpeedMode, L0::globalDriverDispatch.sysmanFan.pfnSetFixedSpeedMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetSpeedTableMode, L0::globalDriverDispatch.sysmanFan.pfnSetSpeedTableMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::globalDriverDispatch.sysmanFan.pfnGetState, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFirmwareProcAddrTable(
    ze_api_version_t version,
    zes_firmware_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanFirmware.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnFlash, L0::globalDriverDispatch.sysmanFirmware.pfnFlash, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetFlashProgress, L0::globalDriverDispatch.sysmanFirmware.pfnGetFlashProgress, version, ZE_API_VERSION_1_8);
    fillDdiEntry(pDdiTable->pfnGetConsoleLogs, L0::globalDriverDispatch.sysmanFirmware.pfnGetConsoleLogs, version, ZE_API_VERSION_1_9);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFrequencyProcAddrTable(
    ze_api_version_t version,
    zes_frequency_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanFrequency.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetAvailableClocks, L0::globalDriverDispatch.sysmanFrequency.pfnGetAvailableClocks, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetRange, L0::globalDriverDispatch.sysmanFrequency.pfnGetRange, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetRange, L0::globalDriverDispatch.sysmanFrequency.pfnSetRange, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::globalDriverDispatch.sysmanFrequency.pfnGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetThrottleTime, L0::globalDriverDispatch.sysmanFrequency.pfnGetThrottleTime, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcGetCapabilities, L0::globalDriverDispatch.sysmanFrequency.pfnOcGetCapabilities, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcGetFrequencyTarget, L0::globalDriverDispatch.sysmanFrequency.pfnOcGetFrequencyTarget, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcSetFrequencyTarget, L0::globalDriverDispatch.sysmanFrequency.pfnOcSetFrequencyTarget, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcGetVoltageTarget, L0::globalDriverDispatch.sysmanFrequency.pfnOcGetVoltageTarget, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcSetVoltageTarget, L0::globalDriverDispatch.sysmanFrequency.pfnOcSetVoltageTarget, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcSetMode, L0::globalDriverDispatch.sysmanFrequency.pfnOcSetMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcGetMode, L0::globalDriverDispatch.sysmanFrequency.pfnOcGetMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcGetIccMax, L0::globalDriverDispatch.sysmanFrequency.pfnOcGetIccMax, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcSetIccMax, L0::globalDriverDispatch.sysmanFrequency.pfnOcSetIccMax, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcGetTjMax, L0::globalDriverDispatch.sysmanFrequency.pfnOcGetTjMax, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOcSetTjMax, L0::globalDriverDispatch.sysmanFrequency.pfnOcSetTjMax, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetLedProcAddrTable(
    ze_api_version_t version,
    zes_led_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanLed.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::globalDriverDispatch.sysmanLed.pfnGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetState, L0::globalDriverDispatch.sysmanLed.pfnSetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetColor, L0::globalDriverDispatch.sysmanLed.pfnSetColor, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetMemoryProcAddrTable(
    ze_api_version_t version,
    zes_memory_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanMemory.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::globalDriverDispatch.sysmanMemory.pfnGetState, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetBandwidth, L0::globalDriverDispatch.sysmanMemory.pfnGetBandwidth, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetPerformanceFactorProcAddrTable(
    ze_api_version_t version,
    zes_performance_factor_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanPerformanceFactor.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetConfig, L0::globalDriverDispatch.sysmanPerformanceFactor.pfnGetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetConfig, L0::globalDriverDispatch.sysmanPerformanceFactor.pfnSetConfig, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetPowerProcAddrTable(
    ze_api_version_t version,
    zes_power_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanPower.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetEnergyCounter, L0::globalDriverDispatch.sysmanPower.pfnGetEnergyCounter, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetLimits, L0::globalDriverDispatch.sysmanPower.pfnGetLimits, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetLimits, L0::globalDriverDispatch.sysmanPower.pfnSetLimits, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetEnergyThreshold, L0::globalDriverDispatch.sysmanPower.pfnGetEnergyThreshold, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetEnergyThreshold, L0::globalDriverDispatch.sysmanPower.pfnSetEnergyThreshold, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetLimitsExt, L0::globalDriverDispatch.sysmanPower.pfnGetLimitsExt, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnSetLimitsExt, L0::globalDriverDispatch.sysmanPower.pfnSetLimitsExt, version, ZE_API_VERSION_1_4);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetPsuProcAddrTable(
    ze_api_version_t version,
    zes_psu_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanPsu.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::globalDriverDispatch.sysmanPsu.pfnGetState, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetRasProcAddrTable(
    ze_api_version_t version,
    zes_ras_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanRas.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetConfig, L0::globalDriverDispatch.sysmanRas.pfnGetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetConfig, L0::globalDriverDispatch.sysmanRas.pfnSetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::globalDriverDispatch.sysmanRas.pfnGetState, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetRasExpProcAddrTable(
    ze_api_version_t version,
    zes_ras_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetStateExp, L0::globalDriverDispatch.sysmanRasExp.pfnGetStateExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnClearStateExp, L0::globalDriverDispatch.sysmanRasExp.pfnClearStateExp, version, ZE_API_VERSION_1_7);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL zesGetDriverExpProcAddrTable(
    ze_api_version_t version, zes_driver_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetDeviceByUuidExp, L0::globalDriverDispatch.sysmanDriverExp.pfnGetDeviceByUuidExp, version, ZE_API_VERSION_1_9);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL zesGetDeviceExpProcAddrTable(
    ze_api_version_t version,
    zes_device_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetSubDevicePropertiesExp, L0::globalDriverDispatch.sysmanDeviceExp.pfnGetSubDevicePropertiesExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnEnumActiveVFExp, L0::globalDriverDispatch.sysmanDeviceExp.pfnEnumActiveVFExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnEnumEnabledVFExp, L0::globalDriverDispatch.sysmanDeviceExp.pfnEnumEnabledVFExp, version, ZE_API_VERSION_1_11);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetVFManagementExpProcAddrTable(
    ze_api_version_t version,
    zes_vf_management_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetVFPropertiesExp, L0::globalDriverDispatch.sysmanVFManagementExp.pfnGetVFPropertiesExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetVFMemoryUtilizationExp, L0::globalDriverDispatch.sysmanVFManagementExp.pfnGetVFMemoryUtilizationExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetVFEngineUtilizationExp, L0::globalDriverDispatch.sysmanVFManagementExp.pfnGetVFEngineUtilizationExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnSetVFTelemetryModeExp, L0::globalDriverDispatch.sysmanVFManagementExp.pfnSetVFTelemetryModeExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnSetVFTelemetrySamplingIntervalExp, L0::globalDriverDispatch.sysmanVFManagementExp.pfnSetVFTelemetrySamplingIntervalExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetVFCapabilitiesExp, L0::globalDriverDispatch.sysmanVFManagementExp.pfnGetVFCapabilitiesExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnGetVFMemoryUtilizationExp2, L0::globalDriverDispatch.sysmanVFManagementExp.pfnGetVFMemoryUtilizationExp2, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnGetVFEngineUtilizationExp2, L0::globalDriverDispatch.sysmanVFManagementExp.pfnGetVFEngineUtilizationExp2, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnGetVFCapabilitiesExp2, L0::globalDriverDispatch.sysmanVFManagementExp.pfnGetVFCapabilitiesExp2, version, ZE_API_VERSION_1_12);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL zesGetFirmwareExpProcAddrTable(
    ze_api_version_t version,
    zes_firmware_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetSecurityVersionExp, L0::globalDriverDispatch.sysmanFirmwareExp.pfnGetSecurityVersionExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnSetSecurityVersionExp, L0::globalDriverDispatch.sysmanFirmwareExp.pfnSetSecurityVersionExp, version, ZE_API_VERSION_1_9);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetSchedulerProcAddrTable(
    ze_api_version_t version,
    zes_scheduler_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanScheduler.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetCurrentMode, L0::globalDriverDispatch.sysmanScheduler.pfnGetCurrentMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetTimeoutModeProperties, L0::globalDriverDispatch.sysmanScheduler.pfnGetTimeoutModeProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetTimesliceModeProperties, L0::globalDriverDispatch.sysmanScheduler.pfnGetTimesliceModeProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetTimeoutMode, L0::globalDriverDispatch.sysmanScheduler.pfnSetTimeoutMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetTimesliceMode, L0::globalDriverDispatch.sysmanScheduler.pfnSetTimesliceMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetExclusiveMode, L0::globalDriverDispatch.sysmanScheduler.pfnSetExclusiveMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetComputeUnitDebugMode, L0::globalDriverDispatch.sysmanScheduler.pfnSetComputeUnitDebugMode, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetStandbyProcAddrTable(
    ze_api_version_t version,
    zes_standby_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanStandby.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetMode, L0::globalDriverDispatch.sysmanStandby.pfnGetMode, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetMode, L0::globalDriverDispatch.sysmanStandby.pfnSetMode, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetTemperatureProcAddrTable(
    ze_api_version_t version,
    zes_temperature_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.sysmanTemperature.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetConfig, L0::globalDriverDispatch.sysmanTemperature.pfnGetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetConfig, L0::globalDriverDispatch.sysmanTemperature.pfnSetConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetState, L0::globalDriverDispatch.sysmanTemperature.pfnGetState, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetOverclockProcAddrTable(
    ze_api_version_t version,           ///< [in] API version requested
    zes_overclock_dditable_t *pDdiTable ///< [in,out] pointer to table of DDI function pointers
) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.sysman.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetDomainProperties, L0::globalDriverDispatch.sysmanOverclock.pfnGetDomainProperties, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetDomainVFProperties, L0::globalDriverDispatch.sysmanOverclock.pfnGetDomainVFProperties, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetDomainControlProperties, L0::globalDriverDispatch.sysmanOverclock.pfnGetDomainControlProperties, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetControlCurrentValue, L0::globalDriverDispatch.sysmanOverclock.pfnGetControlCurrentValue, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetControlPendingValue, L0::globalDriverDispatch.sysmanOverclock.pfnGetControlPendingValue, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnSetControlUserValue, L0::globalDriverDispatch.sysmanOverclock.pfnSetControlUserValue, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetControlState, L0::globalDriverDispatch.sysmanOverclock.pfnGetControlState, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetVFPointValues, L0::globalDriverDispatch.sysmanOverclock.pfnGetVFPointValues, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnSetVFPointValues, L0::globalDriverDispatch.sysmanOverclock.pfnSetVFPointValues, version, ZE_API_VERSION_1_5);

    return result;
}
