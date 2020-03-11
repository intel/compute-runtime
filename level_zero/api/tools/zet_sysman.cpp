/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/zet_api.h>

#include "sysman/sysman.h"

extern "C" {

__zedllexport ze_result_t __zecall
zetSysmanGet(
    zet_device_handle_t hDevice,
    zet_sysman_version_t version,
    zet_sysman_handle_t *phSysman) {
    return L0::SysmanHandleContext::sysmanGet(hDevice, phSysman);
}

__zedllexport ze_result_t __zecall
zetSysmanDeviceGetProperties(
    zet_sysman_handle_t hSysman,
    zet_sysman_properties_t *pProperties) {
    return L0::Sysman::fromHandle(hSysman)->deviceGetProperties(pProperties);
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerGetCurrentMode(
    zet_sysman_handle_t hSysman,
    zet_sched_mode_t *pMode) {
    return L0::Sysman::fromHandle(hSysman)->schedulerGetCurrentMode(pMode);
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerGetTimeoutModeProperties(
    zet_sysman_handle_t hSysman,
    ze_bool_t getDefaults,
    zet_sched_timeout_properties_t *pConfig) {
    return L0::Sysman::fromHandle(hSysman)->schedulerGetTimeoutModeProperties(getDefaults, pConfig);
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerGetTimesliceModeProperties(
    zet_sysman_handle_t hSysman,
    ze_bool_t getDefaults,
    zet_sched_timeslice_properties_t *pConfig) {
    return L0::Sysman::fromHandle(hSysman)->schedulerGetTimesliceModeProperties(getDefaults, pConfig);
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerSetTimeoutMode(
    zet_sysman_handle_t hSysman,
    zet_sched_timeout_properties_t *pProperties,
    ze_bool_t *pNeedReboot) {
    return L0::Sysman::fromHandle(hSysman)->schedulerSetTimeoutMode(pProperties, pNeedReboot);
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerSetTimesliceMode(
    zet_sysman_handle_t hSysman,
    zet_sched_timeslice_properties_t *pProperties,
    ze_bool_t *pNeedReboot) {
    return L0::Sysman::fromHandle(hSysman)->schedulerSetTimesliceMode(pProperties, pNeedReboot);
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerSetExclusiveMode(
    zet_sysman_handle_t hSysman,
    ze_bool_t *pNeedReboot) {
    return L0::Sysman::fromHandle(hSysman)->schedulerSetExclusiveMode(pNeedReboot);
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerSetComputeUnitDebugMode(
    zet_sysman_handle_t hSysman,
    ze_bool_t *pNeedReboot) {
    return L0::Sysman::fromHandle(hSysman)->schedulerSetComputeUnitDebugMode(pNeedReboot);
}

__zedllexport ze_result_t __zecall
zetSysmanProcessesGetState(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_process_state_t *pProcesses) {
    return L0::Sysman::fromHandle(hSysman)->processesGetState(pCount, pProcesses);
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

__zedllexport ze_result_t __zecall
zetSysmanPciGetProperties(
    zet_sysman_handle_t hSysman,
    zet_pci_properties_t *pProperties) {
    return L0::Sysman::fromHandle(hSysman)->pciGetProperties(pProperties);
}

__zedllexport ze_result_t __zecall
zetSysmanPciGetState(
    zet_sysman_handle_t hSysman,
    zet_pci_state_t *pState) {
    return L0::Sysman::fromHandle(hSysman)->pciGetState(pState);
}

__zedllexport ze_result_t __zecall
zetSysmanPciGetBars(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_pci_bar_properties_t *pProperties) {
    return L0::Sysman::fromHandle(hSysman)->pciGetBars(pCount, pProperties);
}

__zedllexport ze_result_t __zecall
zetSysmanPciGetStats(
    zet_sysman_handle_t hSysman,
    zet_pci_stats_t *pStats) {
    return L0::Sysman::fromHandle(hSysman)->pciGetStats(pStats);
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_pwr_handle_t *phPower) {
    return L0::Sysman::fromHandle(hSysman)->powerGet(pCount, phPower);
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGetProperties(
    zet_sysman_pwr_handle_t hPower,
    zet_power_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGetEnergyCounter(
    zet_sysman_pwr_handle_t hPower,
    zet_power_energy_counter_t *pEnergy) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGetLimits(
    zet_sysman_pwr_handle_t hPower,
    zet_power_sustained_limit_t *pSustained,
    zet_power_burst_limit_t *pBurst,
    zet_power_peak_limit_t *pPeak) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPowerSetLimits(
    zet_sysman_pwr_handle_t hPower,
    const zet_power_sustained_limit_t *pSustained,
    const zet_power_burst_limit_t *pBurst,
    const zet_power_peak_limit_t *pPeak) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGetEnergyThreshold(
    zet_sysman_pwr_handle_t hPower,
    zet_energy_threshold_t *pThreshold) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPowerSetEnergyThreshold(
    zet_sysman_pwr_handle_t hPower,
    double threshold) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_freq_handle_t *phFrequency) {
    return L0::Sysman::fromHandle(hSysman)->frequencyGet(pCount, phFrequency);
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetProperties(
    zet_sysman_freq_handle_t hFrequency,
    zet_freq_properties_t *pProperties) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetProperties(pProperties);
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetAvailableClocks(
    zet_sysman_freq_handle_t hFrequency,
    uint32_t *pCount,
    double *phFrequency) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetRange(
    zet_sysman_freq_handle_t hFrequency,
    zet_freq_range_t *pLimits) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetRange(pLimits);
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencySetRange(
    zet_sysman_freq_handle_t hFrequency,
    const zet_freq_range_t *pLimits) {
    return L0::Frequency::fromHandle(hFrequency)->frequencySetRange(pLimits);
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetState(
    zet_sysman_freq_handle_t hFrequency,
    zet_freq_state_t *pState) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetState(pState);
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetThrottleTime(
    zet_sysman_freq_handle_t hFrequency,
    zet_freq_throttle_time_t *pThrottleTime) {
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

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcGetIccMax(
    zet_sysman_freq_handle_t hFrequency,
    double *pOcIccMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcSetIccMax(
    zet_sysman_freq_handle_t hFrequency,
    double ocIccMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcGetTjMax(
    zet_sysman_freq_handle_t hFrequency,
    double *pOcTjMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcSetTjMax(
    zet_sysman_freq_handle_t hFrequency,
    double ocTjMax) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanEngineGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_engine_handle_t *phEngine) {
    return L0::Sysman::fromHandle(hSysman)->engineGet(pCount, phEngine);
}

__zedllexport ze_result_t __zecall
zetSysmanEngineGetProperties(
    zet_sysman_engine_handle_t hEngine,
    zet_engine_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanEngineGetActivity(
    zet_sysman_engine_handle_t hEngine,
    zet_engine_stats_t *pStats) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanStandbyGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_standby_handle_t *phStandby) {
    return L0::Sysman::fromHandle(hSysman)->standbyGet(pCount, phStandby);
}

__zedllexport ze_result_t __zecall
zetSysmanStandbyGetProperties(
    zet_sysman_standby_handle_t hStandby,
    zet_standby_properties_t *pProperties) {
    return L0::Standby::fromHandle(hStandby)->standbyGetProperties(pProperties);
}

__zedllexport ze_result_t __zecall
zetSysmanStandbyGetMode(
    zet_sysman_standby_handle_t hStandby,
    zet_standby_promo_mode_t *pMode) {
    return L0::Standby::fromHandle(hStandby)->standbyGetMode(pMode);
}

__zedllexport ze_result_t __zecall
zetSysmanStandbySetMode(
    zet_sysman_standby_handle_t hStandby,
    zet_standby_promo_mode_t mode) {
    return L0::Standby::fromHandle(hStandby)->standbySetMode(mode);
}

__zedllexport ze_result_t __zecall
zetSysmanFirmwareGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_firmware_handle_t *phFirmware) {
    return L0::Sysman::fromHandle(hSysman)->firmwareGet(pCount, phFirmware);
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

__zedllexport ze_result_t __zecall
zetSysmanFirmwareFlash(
    zet_sysman_firmware_handle_t hFirmware,
    void *pImage,
    uint32_t size) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanMemoryGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_mem_handle_t *phMemory) {
    return L0::Sysman::fromHandle(hSysman)->memoryGet(pCount, phMemory);
}

__zedllexport ze_result_t __zecall
zetSysmanMemoryGetProperties(
    zet_sysman_mem_handle_t hMemory,
    zet_mem_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanMemoryGetState(
    zet_sysman_mem_handle_t hMemory,
    zet_mem_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanMemoryGetBandwidth(
    zet_sysman_mem_handle_t hMemory,
    zet_mem_bandwidth_t *pBandwidth) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_fabric_port_handle_t *phPort) {
    return L0::Sysman::fromHandle(hSysman)->fabricPortGet(pCount, phPort);
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetProperties(
    zet_sysman_fabric_port_handle_t hPort,
    zet_fabric_port_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetLinkType(
    zet_sysman_fabric_port_handle_t hPort,
    ze_bool_t verbose,
    zet_fabric_link_type_t *pLinkType) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetConfig(
    zet_sysman_fabric_port_handle_t hPort,
    zet_fabric_port_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortSetConfig(
    zet_sysman_fabric_port_handle_t hPort,
    const zet_fabric_port_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetState(
    zet_sysman_fabric_port_handle_t hPort,
    zet_fabric_port_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetThroughput(
    zet_sysman_fabric_port_handle_t hPort,
    zet_fabric_port_throughput_t *pThroughput) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_temp_handle_t *phTemperature) {
    return L0::Sysman::fromHandle(hSysman)->temperatureGet(pCount, phTemperature);
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureGetProperties(
    zet_sysman_temp_handle_t hTemperature,
    zet_temp_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureGetConfig(
    zet_sysman_temp_handle_t hTemperature,
    zet_temp_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureSetConfig(
    zet_sysman_temp_handle_t hTemperature,
    const zet_temp_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureGetState(
    zet_sysman_temp_handle_t hTemperature,
    double *pTemperature) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPsuGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_psu_handle_t *phPsu) {
    return L0::Sysman::fromHandle(hSysman)->psuGet(pCount, phPsu);
}

__zedllexport ze_result_t __zecall
zetSysmanPsuGetProperties(
    zet_sysman_psu_handle_t hPsu,
    zet_psu_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanPsuGetState(
    zet_sysman_psu_handle_t hPsu,
    zet_psu_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFanGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_fan_handle_t *phFan) {
    return L0::Sysman::fromHandle(hSysman)->fanGet(pCount, phFan);
}

__zedllexport ze_result_t __zecall
zetSysmanFanGetProperties(
    zet_sysman_fan_handle_t hFan,
    zet_fan_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFanGetConfig(
    zet_sysman_fan_handle_t hFan,
    zet_fan_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFanSetConfig(
    zet_sysman_fan_handle_t hFan,
    const zet_fan_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanFanGetState(
    zet_sysman_fan_handle_t hFan,
    zet_fan_speed_units_t units,
    uint32_t *pSpeed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanLedGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_led_handle_t *phLed) {
    return L0::Sysman::fromHandle(hSysman)->ledGet(pCount, phLed);
}

__zedllexport ze_result_t __zecall
zetSysmanLedGetProperties(
    zet_sysman_led_handle_t hLed,
    zet_led_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanLedGetState(
    zet_sysman_led_handle_t hLed,
    zet_led_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanLedSetState(
    zet_sysman_led_handle_t hLed,
    const zet_led_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanRasGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_ras_handle_t *phRas) {
    return L0::Sysman::fromHandle(hSysman)->rasGet(pCount, phRas);
}

__zedllexport ze_result_t __zecall
zetSysmanRasGetProperties(
    zet_sysman_ras_handle_t hRas,
    zet_ras_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanRasGetConfig(
    zet_sysman_ras_handle_t hRas,
    zet_ras_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanRasSetConfig(
    zet_sysman_ras_handle_t hRas,
    const zet_ras_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanRasGetState(
    zet_sysman_ras_handle_t hRas,
    ze_bool_t clear,
    uint64_t *pTotalErrors,
    zet_ras_details_t *pDetails) {
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

__zedllexport ze_result_t __zecall
zetSysmanEventListen(
    ze_driver_handle_t hDriver,
    uint32_t timeout,
    uint32_t count,
    zet_sysman_event_handle_t *phEvents,
    uint32_t *pEvents) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanDiagnosticsGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_diag_handle_t *phDiagnostics) {
    return L0::Sysman::fromHandle(hSysman)->diagnosticsGet(pCount, phDiagnostics);
}

__zedllexport ze_result_t __zecall
zetSysmanDiagnosticsGetProperties(
    zet_sysman_diag_handle_t hDiagnostics,
    zet_diag_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

__zedllexport ze_result_t __zecall
zetSysmanDiagnosticsGetTests(
    zet_sysman_diag_handle_t hDiagnostics,
    uint32_t *pCount,
    zet_diag_test_t *pTests) {
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

} // extern C
