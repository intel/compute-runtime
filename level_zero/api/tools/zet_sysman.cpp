/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/zet_api.h>

#include "sysman/sysman.h"
#include "third_party/level_zero/zes_api_ext.h"

extern "C" {

__zedllexport ze_result_t __zecall
zetSysmanGet(
    zet_device_handle_t hDevice,
    zet_sysman_version_t version,
    zet_sysman_handle_t *phSysman) {
    return L0::SysmanHandleContext::sysmanGet(hDevice, phSysman);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceGetProperties(
    zes_device_handle_t hDevice,
    zes_device_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanDeviceGetProperties(
    zet_sysman_handle_t hSysman,
    zet_sysman_properties_t *pProperties) {
    return L0::Sysman::fromHandle(hSysman)->deviceGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceGetState(
    zes_device_handle_t hDevice,
    zes_device_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumSchedulers(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_sched_handle_t *phScheduler) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerGetProperties(
    zes_sched_handle_t hScheduler,
    zes_sched_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerGetCurrentMode(
    zes_sched_handle_t hScheduler,
    zes_sched_mode_t *pMode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerGetCurrentMode(
    zet_sysman_handle_t hSysman,
    zet_sched_mode_t *pMode) {
    return L0::Sysman::fromHandle(hSysman)->schedulerGetCurrentMode(pMode);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerGetTimeoutModeProperties(
    zes_sched_handle_t hScheduler,
    ze_bool_t getDefaults,
    zes_sched_timeout_properties_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerGetTimeoutModeProperties(
    zet_sysman_handle_t hSysman,
    ze_bool_t getDefaults,
    zet_sched_timeout_properties_t *pConfig) {
    return L0::Sysman::fromHandle(hSysman)->schedulerGetTimeoutModeProperties(getDefaults, pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerGetTimesliceModeProperties(
    zes_sched_handle_t hScheduler,
    ze_bool_t getDefaults,
    zes_sched_timeslice_properties_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerGetTimesliceModeProperties(
    zet_sysman_handle_t hSysman,
    ze_bool_t getDefaults,
    zet_sched_timeslice_properties_t *pConfig) {
    return L0::Sysman::fromHandle(hSysman)->schedulerGetTimesliceModeProperties(getDefaults, pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerSetTimeoutMode(
    zes_sched_handle_t hScheduler,
    zes_sched_timeout_properties_t *pProperties,
    ze_bool_t *pNeedReload) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerSetTimeoutMode(
    zet_sysman_handle_t hSysman,
    zet_sched_timeout_properties_t *pProperties,
    ze_bool_t *pNeedReboot) {
    return L0::Sysman::fromHandle(hSysman)->schedulerSetTimeoutMode(pProperties, pNeedReboot);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerSetTimesliceMode(
    zes_sched_handle_t hScheduler,
    zes_sched_timeslice_properties_t *pProperties,
    ze_bool_t *pNeedReload) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerSetTimesliceMode(
    zet_sysman_handle_t hSysman,
    zet_sched_timeslice_properties_t *pProperties,
    ze_bool_t *pNeedReboot) {
    return L0::Sysman::fromHandle(hSysman)->schedulerSetTimesliceMode(pProperties, pNeedReboot);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerSetExclusiveMode(
    zes_sched_handle_t hScheduler,
    ze_bool_t *pNeedReload) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerSetExclusiveMode(
    zet_sysman_handle_t hSysman,
    ze_bool_t *pNeedReboot) {
    return L0::Sysman::fromHandle(hSysman)->schedulerSetExclusiveMode(pNeedReboot);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerSetComputeUnitDebugMode(
    zes_sched_handle_t hScheduler,
    ze_bool_t *pNeedReload) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerSetComputeUnitDebugMode(
    zet_sysman_handle_t hSysman,
    ze_bool_t *pNeedReboot) {
    return L0::Sysman::fromHandle(hSysman)->schedulerSetComputeUnitDebugMode(pNeedReboot);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceProcessesGetState(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_process_state_t *pProcesses) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanProcessesGetState(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_process_state_t *pProcesses) {
    return L0::Sysman::fromHandle(hSysman)->processesGetState(pCount, pProcesses);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceReset(
    zes_device_handle_t hDevice,
    ze_bool_t force) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanDeviceReset(
    zet_sysman_handle_t hSysman) {
    return L0::Sysman::fromHandle(hSysman)->deviceReset();
}

__zedllexport ze_result_t __zecall
zetSysmanDeviceGetRepairStatus(
    zet_sysman_handle_t hSysman,
    zet_repair_status_t *pRepairStatus) {
    return L0::Sysman::fromHandle(hSysman)->deviceGetRepairStatus(pRepairStatus);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDevicePciGetProperties(
    zes_device_handle_t hDevice,
    zes_pci_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPciGetProperties(
    zet_sysman_handle_t hSysman,
    zet_pci_properties_t *pProperties) {
    return L0::Sysman::fromHandle(hSysman)->pciGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDevicePciGetState(
    zes_device_handle_t hDevice,
    zes_pci_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPciGetState(
    zet_sysman_handle_t hSysman,
    zet_pci_state_t *pState) {
    return L0::Sysman::fromHandle(hSysman)->pciGetState(pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDevicePciGetBars(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_pci_bar_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPciGetBars(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_pci_bar_properties_t *pProperties) {
    return L0::Sysman::fromHandle(hSysman)->pciGetBars(pCount, pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDevicePciGetStats(
    zes_device_handle_t hDevice,
    zes_pci_stats_t *pStats) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPciGetStats(
    zet_sysman_handle_t hSysman,
    zet_pci_stats_t *pStats) {
    return L0::Sysman::fromHandle(hSysman)->pciGetStats(pStats);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumPowerDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_pwr_handle_t *phPower) {
    return L0::SysmanDevice::fromHandle(hDevice)->powerGet(pCount, phPower);
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_pwr_handle_t *phPower) {
    return L0::Sysman::fromHandle(hSysman)->powerGet(pCount, phPower);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPowerGetProperties(
    zes_pwr_handle_t hPower,
    zes_power_properties_t *pProperties) {
    return L0::Power::fromHandle(hPower)->powerGetProperties(pProperties);
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGetProperties(
    zet_sysman_pwr_handle_t hPower,
    zet_power_properties_t *pProperties) {
    return L0::Power::fromHandle(hPower)->powerGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPowerGetEnergyCounter(
    zes_pwr_handle_t hPower,
    zes_power_energy_counter_t *pEnergy) {
    return L0::Power::fromHandle(hPower)->powerGetEnergyCounter(pEnergy);
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGetEnergyCounter(
    zet_sysman_pwr_handle_t hPower,
    zet_power_energy_counter_t *pEnergy) {
    return L0::Power::fromHandle(hPower)->powerGetEnergyCounter(pEnergy);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPowerGetLimits(
    zes_pwr_handle_t hPower,
    zes_power_sustained_limit_t *pSustained,
    zes_power_burst_limit_t *pBurst,
    zes_power_peak_limit_t *pPeak) {
    return L0::Power::fromHandle(hPower)->powerGetLimits(pSustained, pBurst, pPeak);
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGetLimits(
    zet_sysman_pwr_handle_t hPower,
    zet_power_sustained_limit_t *pSustained,
    zet_power_burst_limit_t *pBurst,
    zet_power_peak_limit_t *pPeak) {
    return L0::Power::fromHandle(hPower)->powerGetLimits(pSustained, pBurst, pPeak);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPowerSetLimits(
    zes_pwr_handle_t hPower,
    const zes_power_sustained_limit_t *pSustained,
    const zes_power_burst_limit_t *pBurst,
    const zes_power_peak_limit_t *pPeak) {
    return L0::Power::fromHandle(hPower)->powerSetLimits(pSustained, pBurst, pPeak);
}

__zedllexport ze_result_t __zecall
zetSysmanPowerSetLimits(
    zet_sysman_pwr_handle_t hPower,
    const zet_power_sustained_limit_t *pSustained,
    const zet_power_burst_limit_t *pBurst,
    const zet_power_peak_limit_t *pPeak) {
    return L0::Power::fromHandle(hPower)->powerSetLimits(pSustained, pBurst, pPeak);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPowerGetEnergyThreshold(
    zes_pwr_handle_t hPower,
    zes_energy_threshold_t *pThreshold) {
    return L0::Power::fromHandle(hPower)->powerGetEnergyThreshold(pThreshold);
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGetEnergyThreshold(
    zet_sysman_pwr_handle_t hPower,
    zet_energy_threshold_t *pThreshold) {
    return L0::Power::fromHandle(hPower)->powerGetEnergyThreshold(pThreshold);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPowerSetEnergyThreshold(
    zes_pwr_handle_t hPower,
    double threshold) {
    return L0::Power::fromHandle(hPower)->powerSetEnergyThreshold(threshold);
}

__zedllexport ze_result_t __zecall
zetSysmanPowerSetEnergyThreshold(
    zet_sysman_pwr_handle_t hPower,
    double threshold) {
    return L0::Power::fromHandle(hPower)->powerSetEnergyThreshold(threshold);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumFrequencyDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_freq_handle_t *phFrequency) {
    return L0::SysmanDevice::fromHandle(hDevice)->frequencyGet(pCount, phFrequency);
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_freq_handle_t *phFrequency) {
    return L0::Sysman::fromHandle(hSysman)->frequencyGet(pCount, phFrequency);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyGetProperties(
    zes_freq_handle_t hFrequency,
    zes_freq_properties_t *pProperties) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetProperties(pProperties);
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetProperties(
    zet_sysman_freq_handle_t hFrequency,
    zet_freq_properties_t *pProperties) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyGetAvailableClocks(
    zes_freq_handle_t hFrequency,
    uint32_t *pCount,
    double *phFrequency) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetAvailableClocks(pCount, phFrequency);
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetAvailableClocks(
    zet_sysman_freq_handle_t hFrequency,
    uint32_t *pCount,
    double *phFrequency) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetAvailableClocks(pCount, phFrequency);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyGetRange(
    zes_freq_handle_t hFrequency,
    zes_freq_range_t *pLimits) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetRange(pLimits);
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetRange(
    zet_sysman_freq_handle_t hFrequency,
    zet_freq_range_t *pLimits) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetRange(pLimits);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencySetRange(
    zes_freq_handle_t hFrequency,
    const zes_freq_range_t *pLimits) {
    return L0::Frequency::fromHandle(hFrequency)->frequencySetRange(pLimits);
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencySetRange(
    zet_sysman_freq_handle_t hFrequency,
    const zet_freq_range_t *pLimits) {
    return L0::Frequency::fromHandle(hFrequency)->frequencySetRange(pLimits);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyGetState(
    zes_freq_handle_t hFrequency,
    zes_freq_state_t *pState) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetState(pState);
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetState(
    zet_sysman_freq_handle_t hFrequency,
    zet_freq_state_t *pState) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetState(pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyGetThrottleTime(
    zes_freq_handle_t hFrequency,
    zes_freq_throttle_time_t *pThrottleTime) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetThrottleTime(pThrottleTime);
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetThrottleTime(
    zet_sysman_freq_handle_t hFrequency,
    zet_freq_throttle_time_t *pThrottleTime) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcGetFrequencyTarget(
    zes_freq_handle_t hFrequency,
    double *pCurrentOcFrequency) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcSetFrequencyTarget(
    zes_freq_handle_t hFrequency,
    double currentOcFrequency) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcGetVoltageTarget(
    zes_freq_handle_t hFrequency,
    double *pCurrentVoltageTarget,
    double *pCurrentVoltageOffset) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcSetVoltageTarget(
    zes_freq_handle_t hFrequency,
    double currentVoltageTarget,
    double currentVoltageOffset) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcSetMode(
    zes_freq_handle_t hFrequency,
    zes_oc_mode_t currentOcMode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcGetMode(
    zes_freq_handle_t hFrequency,
    zes_oc_mode_t *pCurrentOcMode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcGetCapabilities(
    zes_freq_handle_t hFrequency,
    zes_oc_capabilities_t *pOcCapabilities) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcGetCapabilities(
    zet_sysman_freq_handle_t hFrequency,
    zet_oc_capabilities_t *pOcCapabilities) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcGetConfig(
    zet_sysman_freq_handle_t hFrequency,
    zet_oc_config_t *pOcConfiguration) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcSetConfig(
    zet_sysman_freq_handle_t hFrequency,
    zet_oc_config_t *pOcConfiguration,
    ze_bool_t *pDeviceRestart) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcGetIccMax(
    zes_freq_handle_t hFrequency,
    double *pOcIccMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcGetIccMax(
    zet_sysman_freq_handle_t hFrequency,
    double *pOcIccMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcSetIccMax(
    zes_freq_handle_t hFrequency,
    double ocIccMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcSetIccMax(
    zet_sysman_freq_handle_t hFrequency,
    double ocIccMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcGetTjMax(
    zes_freq_handle_t hFrequency,
    double *pOcTjMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcGetTjMax(
    zet_sysman_freq_handle_t hFrequency,
    double *pOcTjMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcSetTjMax(
    zes_freq_handle_t hFrequency,
    double ocTjMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcSetTjMax(
    zet_sysman_freq_handle_t hFrequency,
    double ocTjMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumEngineGroups(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_engine_handle_t *phEngine) {
    return L0::SysmanDevice::fromHandle(hDevice)->engineGet(pCount, phEngine);
}

__zedllexport ze_result_t __zecall
zetSysmanEngineGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_engine_handle_t *phEngine) {
    return L0::Sysman::fromHandle(hSysman)->engineGet(pCount, phEngine);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesEngineGetProperties(
    zes_engine_handle_t hEngine,
    zes_engine_properties_t *pProperties) {
    return L0::Engine::fromHandle(hEngine)->engineGetProperties(pProperties);
}

__zedllexport ze_result_t __zecall
zetSysmanEngineGetProperties(
    zet_sysman_engine_handle_t hEngine,
    zet_engine_properties_t *pProperties) {
    return L0::Engine::fromHandle(hEngine)->engineGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesEngineGetActivity(
    zes_engine_handle_t hEngine,
    zes_engine_stats_t *pStats) {
    return L0::Engine::fromHandle(hEngine)->engineGetActivity(pStats);
}

__zedllexport ze_result_t __zecall
zetSysmanEngineGetActivity(
    zet_sysman_engine_handle_t hEngine,
    zet_engine_stats_t *pStats) {
    return L0::Engine::fromHandle(hEngine)->engineGetActivity(pStats);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumStandbyDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_standby_handle_t *phStandby) {
    return L0::SysmanDevice::fromHandle(hDevice)->standbyGet(pCount, phStandby);
}

__zedllexport ze_result_t __zecall
zetSysmanStandbyGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_standby_handle_t *phStandby) {
    return L0::Sysman::fromHandle(hSysman)->standbyGet(pCount, phStandby);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesStandbyGetProperties(
    zes_standby_handle_t hStandby,
    zes_standby_properties_t *pProperties) {
    return L0::Standby::fromHandle(hStandby)->standbyGetProperties(pProperties);
}

__zedllexport ze_result_t __zecall
zetSysmanStandbyGetProperties(
    zet_sysman_standby_handle_t hStandby,
    zet_standby_properties_t *pProperties) {
    return L0::Standby::fromHandle(hStandby)->standbyGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesStandbyGetMode(
    zes_standby_handle_t hStandby,
    zes_standby_promo_mode_t *pMode) {
    return L0::Standby::fromHandle(hStandby)->standbyGetMode(pMode);
}

__zedllexport ze_result_t __zecall
zetSysmanStandbyGetMode(
    zet_sysman_standby_handle_t hStandby,
    zet_standby_promo_mode_t *pMode) {
    return L0::Standby::fromHandle(hStandby)->standbyGetMode(pMode);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesStandbySetMode(
    zes_standby_handle_t hStandby,
    zes_standby_promo_mode_t mode) {
    return L0::Standby::fromHandle(hStandby)->standbySetMode(mode);
}

__zedllexport ze_result_t __zecall
zetSysmanStandbySetMode(
    zet_sysman_standby_handle_t hStandby,
    zet_standby_promo_mode_t mode) {
    return L0::Standby::fromHandle(hStandby)->standbySetMode(mode);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumFirmwares(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_firmware_handle_t *phFirmware) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFirmwareGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_firmware_handle_t *phFirmware) {
    return L0::Sysman::fromHandle(hSysman)->firmwareGet(pCount, phFirmware);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFirmwareGetProperties(
    zes_firmware_handle_t hFirmware,
    zes_firmware_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFirmwareGetProperties(
    zet_sysman_firmware_handle_t hFirmware,
    zet_firmware_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFirmwareGetChecksum(
    zet_sysman_firmware_handle_t hFirmware,
    uint32_t *pChecksum) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFirmwareFlash(
    zes_firmware_handle_t hFirmware,
    void *pImage,
    uint32_t size) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFirmwareFlash(
    zet_sysman_firmware_handle_t hFirmware,
    void *pImage,
    uint32_t size) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumMemoryModules(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_mem_handle_t *phMemory) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanMemoryGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_mem_handle_t *phMemory) {
    return L0::Sysman::fromHandle(hSysman)->memoryGet(pCount, phMemory);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesMemoryGetProperties(
    zes_mem_handle_t hMemory,
    zes_mem_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanMemoryGetProperties(
    zet_sysman_mem_handle_t hMemory,
    zet_mem_properties_t *pProperties) {
    return L0::Memory::fromHandle(hMemory)->memoryGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesMemoryGetState(
    zes_mem_handle_t hMemory,
    zes_mem_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanMemoryGetState(
    zet_sysman_mem_handle_t hMemory,
    zet_mem_state_t *pState) {
    return L0::Memory::fromHandle(hMemory)->memoryGetState(pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesMemoryGetBandwidth(
    zes_mem_handle_t hMemory,
    zes_mem_bandwidth_t *pBandwidth) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanMemoryGetBandwidth(
    zet_sysman_mem_handle_t hMemory,
    zet_mem_bandwidth_t *pBandwidth) {
    return L0::Memory::fromHandle(hMemory)->memoryGetBandwidth(pBandwidth);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumFabricPorts(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_fabric_port_handle_t *phPort) {
    return L0::SysmanDevice::fromHandle(hDevice)->fabricPortGet(pCount, phPort);
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_fabric_port_handle_t *phPort) {
    return L0::Sysman::fromHandle(hSysman)->fabricPortGet(pCount, phPort);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFabricPortGetProperties(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_properties_t *pProperties) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetProperties(pProperties);
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetProperties(
    zet_sysman_fabric_port_handle_t hPort,
    zet_fabric_port_properties_t *pProperties) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFabricPortGetLinkType(
    zes_fabric_port_handle_t hPort,
    zes_fabric_link_type_t *pLinkType) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetLinkType(pLinkType);
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetLinkType(
    zet_sysman_fabric_port_handle_t hPort,
    ze_bool_t verbose,
    zet_fabric_link_type_t *pLinkType) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetLinkType(verbose, pLinkType);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFabricPortGetConfig(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_config_t *pConfig) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetConfig(pConfig);
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetConfig(
    zet_sysman_fabric_port_handle_t hPort,
    zet_fabric_port_config_t *pConfig) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetConfig(pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFabricPortSetConfig(
    zes_fabric_port_handle_t hPort,
    const zes_fabric_port_config_t *pConfig) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortSetConfig(pConfig);
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortSetConfig(
    zet_sysman_fabric_port_handle_t hPort,
    const zet_fabric_port_config_t *pConfig) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortSetConfig(pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFabricPortGetState(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_state_t *pState) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetState(pState);
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetState(
    zet_sysman_fabric_port_handle_t hPort,
    zet_fabric_port_state_t *pState) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetState(pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFabricPortGetThroughput(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_throughput_t *pThroughput) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetThroughput(pThroughput);
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetThroughput(
    zet_sysman_fabric_port_handle_t hPort,
    zet_fabric_port_throughput_t *pThroughput) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetThroughput(pThroughput);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumTemperatureSensors(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_temp_handle_t *phTemperature) {
    return L0::SysmanDevice::fromHandle(hDevice)->temperatureGet(pCount, phTemperature);
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_temp_handle_t *phTemperature) {
    return L0::Sysman::fromHandle(hSysman)->temperatureGet(pCount, phTemperature);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesTemperatureGetProperties(
    zes_temp_handle_t hTemperature,
    zes_temp_properties_t *pProperties) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureGetProperties(pProperties);
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureGetProperties(
    zet_sysman_temp_handle_t hTemperature,
    zet_temp_properties_t *pProperties) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesTemperatureGetConfig(
    zes_temp_handle_t hTemperature,
    zes_temp_config_t *pConfig) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureGetConfig(pConfig);
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureGetConfig(
    zet_sysman_temp_handle_t hTemperature,
    zet_temp_config_t *pConfig) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureGetConfig(pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesTemperatureSetConfig(
    zes_temp_handle_t hTemperature,
    const zes_temp_config_t *pConfig) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureSetConfig(pConfig);
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureSetConfig(
    zet_sysman_temp_handle_t hTemperature,
    const zet_temp_config_t *pConfig) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureSetConfig(pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesTemperatureGetState(
    zes_temp_handle_t hTemperature,
    double *pTemperature) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureGetState(pTemperature);
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureGetState(
    zet_sysman_temp_handle_t hTemperature,
    double *pTemperature) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureGetState(pTemperature);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumPsus(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_psu_handle_t *phPsu) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPsuGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_psu_handle_t *phPsu) {
    return L0::Sysman::fromHandle(hSysman)->psuGet(pCount, phPsu);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPsuGetProperties(
    zes_psu_handle_t hPsu,
    zes_psu_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPsuGetProperties(
    zet_sysman_psu_handle_t hPsu,
    zet_psu_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPsuGetState(
    zes_psu_handle_t hPsu,
    zes_psu_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPsuGetState(
    zet_sysman_psu_handle_t hPsu,
    zet_psu_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumFans(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_fan_handle_t *phFan) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFanGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_fan_handle_t *phFan) {
    return L0::Sysman::fromHandle(hSysman)->fanGet(pCount, phFan);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFanGetProperties(
    zes_fan_handle_t hFan,
    zes_fan_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFanGetProperties(
    zet_sysman_fan_handle_t hFan,
    zet_fan_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFanGetConfig(
    zes_fan_handle_t hFan,
    zes_fan_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFanGetConfig(
    zet_sysman_fan_handle_t hFan,
    zet_fan_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFanSetDefaultMode(
    zes_fan_handle_t hFan) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFanSetFixedSpeedMode(
    zes_fan_handle_t hFan,
    const zes_fan_speed_t *speed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFanSetSpeedTableMode(
    zes_fan_handle_t hFan,
    const zes_fan_speed_table_t *speedTable) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFanSetConfig(
    zet_sysman_fan_handle_t hFan,
    const zet_fan_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFanGetState(
    zes_fan_handle_t hFan,
    zes_fan_speed_units_t units,
    int32_t *pSpeed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFanGetState(
    zet_sysman_fan_handle_t hFan,
    zet_fan_speed_units_t units,
    uint32_t *pSpeed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumLeds(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_led_handle_t *phLed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanLedGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_led_handle_t *phLed) {
    return L0::Sysman::fromHandle(hSysman)->ledGet(pCount, phLed);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesLedGetProperties(
    zes_led_handle_t hLed,
    zes_led_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanLedGetProperties(
    zet_sysman_led_handle_t hLed,
    zet_led_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesLedGetState(
    zes_led_handle_t hLed,
    zes_led_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanLedGetState(
    zet_sysman_led_handle_t hLed,
    zet_led_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesLedSetState(
    zes_led_handle_t hLed,
    ze_bool_t enable) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanLedSetState(
    zet_sysman_led_handle_t hLed,
    const zet_led_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesLedSetColor(
    zes_led_handle_t hLed,
    const zes_led_color_t *pColor) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumRasErrorSets(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_ras_handle_t *phRas) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanRasGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_ras_handle_t *phRas) {
    return L0::Sysman::fromHandle(hSysman)->rasGet(pCount, phRas);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesRasGetProperties(
    zes_ras_handle_t hRas,
    zes_ras_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanRasGetProperties(
    zet_sysman_ras_handle_t hRas,
    zet_ras_properties_t *pProperties) {
    return L0::Ras::fromHandle(hRas)->rasGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesRasGetConfig(
    zes_ras_handle_t hRas,
    zes_ras_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanRasGetConfig(
    zet_sysman_ras_handle_t hRas,
    zet_ras_config_t *pConfig) {
    return L0::Ras::fromHandle(hRas)->rasGetConfig(pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesRasSetConfig(
    zes_ras_handle_t hRas,
    const zes_ras_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanRasSetConfig(
    zet_sysman_ras_handle_t hRas,
    const zet_ras_config_t *pConfig) {
    return L0::Ras::fromHandle(hRas)->rasSetConfig(pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesRasGetState(
    zes_ras_handle_t hRas,
    ze_bool_t clear,
    zes_ras_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanRasGetState(
    zet_sysman_ras_handle_t hRas,
    ze_bool_t clear,
    uint64_t *pTotalErrors,
    zet_ras_details_t *pDetails) {
    return L0::Ras::fromHandle(hRas)->rasGetState(clear, pTotalErrors, pDetails);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEventRegister(
    zes_device_handle_t hDevice,
    zes_event_type_flags_t events) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanEventGet(
    zet_sysman_handle_t hSysman,
    zet_sysman_event_handle_t *phEvent) {
    return L0::Sysman::fromHandle(hSysman)->eventGet(phEvent);
}

__zedllexport ze_result_t __zecall
zetSysmanEventGetConfig(
    zet_sysman_event_handle_t hEvent,
    zet_event_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanEventSetConfig(
    zet_sysman_event_handle_t hEvent,
    const zet_event_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanEventGetState(
    zet_sysman_event_handle_t hEvent,
    ze_bool_t clear,
    uint32_t *pEvents) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDriverEventListen(
    ze_driver_handle_t hDriver,
    uint32_t timeout,
    uint32_t count,
    zes_device_handle_t *phDevices,
    uint32_t *pNumDeviceEvents,
    zes_event_type_flags_t *pEvents) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanEventListen(
    ze_driver_handle_t hDriver,
    uint32_t timeout,
    uint32_t count,
    zet_sysman_event_handle_t *phEvents,
    uint32_t *pEvents) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumDiagnosticTestSuites(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_diag_handle_t *phDiagnostics) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanDiagnosticsGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_diag_handle_t *phDiagnostics) {
    return L0::Sysman::fromHandle(hSysman)->diagnosticsGet(pCount, phDiagnostics);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDiagnosticsGetProperties(
    zes_diag_handle_t hDiagnostics,
    zes_diag_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanDiagnosticsGetProperties(
    zet_sysman_diag_handle_t hDiagnostics,
    zet_diag_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDiagnosticsGetTests(
    zes_diag_handle_t hDiagnostics,
    uint32_t *pCount,
    zes_diag_test_t *pTests) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanDiagnosticsGetTests(
    zet_sysman_diag_handle_t hDiagnostics,
    uint32_t *pCount,
    zet_diag_test_t *pTests) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDiagnosticsRunTests(
    zes_diag_handle_t hDiagnostics,
    uint32_t start,
    uint32_t end,
    zes_diag_result_t *pResult) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanDiagnosticsRunTests(
    zet_sysman_diag_handle_t hDiagnostics,
    uint32_t start,
    uint32_t end,
    zet_diag_result_t *pResult) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumPerformanceFactorDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_perf_handle_t *phPerf) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPerformanceFactorGetProperties(
    zes_perf_handle_t hPerf,
    zes_perf_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPerformanceFactorGetConfig(
    zes_perf_handle_t hPerf,
    double *pFactor) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPerformanceFactorSetConfig(
    zes_perf_handle_t hPerf,
    double factor) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // extern C
