/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/sysman.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceGetProperties(
    zes_device_handle_t hDevice,
    zes_device_properties_t *pProperties) {
    return L0::SysmanDevice::deviceGetProperties(hDevice, pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceGetState(
    zes_device_handle_t hDevice,
    zes_device_state_t *pState) {
    return L0::SysmanDevice::deviceGetState(hDevice, pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumSchedulers(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_sched_handle_t *phScheduler) {
    return L0::SysmanDevice::schedulerGet(hDevice, pCount, phScheduler);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerGetProperties(
    zes_sched_handle_t hScheduler,
    zes_sched_properties_t *pProperties) {
    return L0::Scheduler::fromHandle(hScheduler)->schedulerGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerGetCurrentMode(
    zes_sched_handle_t hScheduler,
    zes_sched_mode_t *pMode) {
    return L0::Scheduler::fromHandle(hScheduler)->getCurrentMode(pMode);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerGetTimeoutModeProperties(
    zes_sched_handle_t hScheduler,
    ze_bool_t getDefaults,
    zes_sched_timeout_properties_t *pConfig) {
    return L0::Scheduler::fromHandle(hScheduler)->getTimeoutModeProperties(getDefaults, pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerGetTimesliceModeProperties(
    zes_sched_handle_t hScheduler,
    ze_bool_t getDefaults,
    zes_sched_timeslice_properties_t *pConfig) {
    return L0::Scheduler::fromHandle(hScheduler)->getTimesliceModeProperties(getDefaults, pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerSetTimeoutMode(
    zes_sched_handle_t hScheduler,
    zes_sched_timeout_properties_t *pProperties,
    ze_bool_t *pNeedReload) {
    return L0::Scheduler::fromHandle(hScheduler)->setTimeoutMode(pProperties, pNeedReload);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerSetTimesliceMode(
    zes_sched_handle_t hScheduler,
    zes_sched_timeslice_properties_t *pProperties,
    ze_bool_t *pNeedReload) {
    return L0::Scheduler::fromHandle(hScheduler)->setTimesliceMode(pProperties, pNeedReload);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerSetExclusiveMode(
    zes_sched_handle_t hScheduler,
    ze_bool_t *pNeedReload) {
    return L0::Scheduler::fromHandle(hScheduler)->setExclusiveMode(pNeedReload);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesSchedulerSetComputeUnitDebugMode(
    zes_sched_handle_t hScheduler,
    ze_bool_t *pNeedReload) {
    return L0::Scheduler::fromHandle(hScheduler)->setComputeUnitDebugMode(pNeedReload);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceProcessesGetState(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_process_state_t *pProcesses) {
    return L0::SysmanDevice::processesGetState(hDevice, pCount, pProcesses);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceReset(
    zes_device_handle_t hDevice,
    ze_bool_t force) {
    return L0::SysmanDevice::deviceReset(hDevice, force);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDevicePciGetProperties(
    zes_device_handle_t hDevice,
    zes_pci_properties_t *pProperties) {
    return L0::SysmanDevice::pciGetProperties(hDevice, pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDevicePciGetState(
    zes_device_handle_t hDevice,
    zes_pci_state_t *pState) {
    return L0::SysmanDevice::pciGetState(hDevice, pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDevicePciGetBars(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_pci_bar_properties_t *pProperties) {
    return L0::SysmanDevice::pciGetBars(hDevice, pCount, pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDevicePciGetStats(
    zes_device_handle_t hDevice,
    zes_pci_stats_t *pStats) {
    return L0::SysmanDevice::pciGetStats(hDevice, pStats);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumPowerDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_pwr_handle_t *phPower) {
    return L0::SysmanDevice::powerGet(hDevice, pCount, phPower);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPowerGetProperties(
    zes_pwr_handle_t hPower,
    zes_power_properties_t *pProperties) {
    return L0::Power::fromHandle(hPower)->powerGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPowerGetEnergyCounter(
    zes_pwr_handle_t hPower,
    zes_power_energy_counter_t *pEnergy) {
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

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPowerSetLimits(
    zes_pwr_handle_t hPower,
    const zes_power_sustained_limit_t *pSustained,
    const zes_power_burst_limit_t *pBurst,
    const zes_power_peak_limit_t *pPeak) {
    return L0::Power::fromHandle(hPower)->powerSetLimits(pSustained, pBurst, pPeak);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPowerGetEnergyThreshold(
    zes_pwr_handle_t hPower,
    zes_energy_threshold_t *pThreshold) {
    return L0::Power::fromHandle(hPower)->powerGetEnergyThreshold(pThreshold);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPowerSetEnergyThreshold(
    zes_pwr_handle_t hPower,
    double threshold) {
    return L0::Power::fromHandle(hPower)->powerSetEnergyThreshold(threshold);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumFrequencyDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_freq_handle_t *phFrequency) {
    return L0::SysmanDevice::frequencyGet(hDevice, pCount, phFrequency);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyGetProperties(
    zes_freq_handle_t hFrequency,
    zes_freq_properties_t *pProperties) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyGetAvailableClocks(
    zes_freq_handle_t hFrequency,
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

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencySetRange(
    zes_freq_handle_t hFrequency,
    const zes_freq_range_t *pLimits) {
    return L0::Frequency::fromHandle(hFrequency)->frequencySetRange(pLimits);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyGetState(
    zes_freq_handle_t hFrequency,
    zes_freq_state_t *pState) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetState(pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyGetThrottleTime(
    zes_freq_handle_t hFrequency,
    zes_freq_throttle_time_t *pThrottleTime) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetThrottleTime(pThrottleTime);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcGetFrequencyTarget(
    zes_freq_handle_t hFrequency,
    double *pCurrentOcFrequency) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcGetFrequencyTarget(pCurrentOcFrequency);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcSetFrequencyTarget(
    zes_freq_handle_t hFrequency,
    double currentOcFrequency) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcSetFrequencyTarget(currentOcFrequency);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcGetVoltageTarget(
    zes_freq_handle_t hFrequency,
    double *pCurrentVoltageTarget,
    double *pCurrentVoltageOffset) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcGetVoltageTarget(pCurrentVoltageTarget, pCurrentVoltageOffset);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcSetVoltageTarget(
    zes_freq_handle_t hFrequency,
    double currentVoltageTarget,
    double currentVoltageOffset) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcSetVoltageTarget(currentVoltageTarget, currentVoltageOffset);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcSetMode(
    zes_freq_handle_t hFrequency,
    zes_oc_mode_t currentOcMode) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcSetMode(currentOcMode);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcGetMode(
    zes_freq_handle_t hFrequency,
    zes_oc_mode_t *pCurrentOcMode) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcGetMode(pCurrentOcMode);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcGetCapabilities(
    zes_freq_handle_t hFrequency,
    zes_oc_capabilities_t *pOcCapabilities) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcGetCapabilities(pOcCapabilities);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcGetIccMax(
    zes_freq_handle_t hFrequency,
    double *pOcIccMax) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcGetIccMax(pOcIccMax);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcSetIccMax(
    zes_freq_handle_t hFrequency,
    double ocIccMax) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcSetIccMax(ocIccMax);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcGetTjMax(
    zes_freq_handle_t hFrequency,
    double *pOcTjMax) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcGeTjMax(pOcTjMax);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFrequencyOcSetTjMax(
    zes_freq_handle_t hFrequency,
    double ocTjMax) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcSetTjMax(ocTjMax);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumEngineGroups(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_engine_handle_t *phEngine) {
    return L0::SysmanDevice::engineGet(hDevice, pCount, phEngine);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesEngineGetProperties(
    zes_engine_handle_t hEngine,
    zes_engine_properties_t *pProperties) {
    return L0::Engine::fromHandle(hEngine)->engineGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesEngineGetActivity(
    zes_engine_handle_t hEngine,
    zes_engine_stats_t *pStats) {
    return L0::Engine::fromHandle(hEngine)->engineGetActivity(pStats);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumStandbyDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_standby_handle_t *phStandby) {
    return L0::SysmanDevice::standbyGet(hDevice, pCount, phStandby);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesStandbyGetProperties(
    zes_standby_handle_t hStandby,
    zes_standby_properties_t *pProperties) {
    return L0::Standby::fromHandle(hStandby)->standbyGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesStandbyGetMode(
    zes_standby_handle_t hStandby,
    zes_standby_promo_mode_t *pMode) {
    return L0::Standby::fromHandle(hStandby)->standbyGetMode(pMode);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesStandbySetMode(
    zes_standby_handle_t hStandby,
    zes_standby_promo_mode_t mode) {
    return L0::Standby::fromHandle(hStandby)->standbySetMode(mode);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumFirmwares(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_firmware_handle_t *phFirmware) {
    return L0::SysmanDevice::firmwareGet(hDevice, pCount, phFirmware);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFirmwareGetProperties(
    zes_firmware_handle_t hFirmware,
    zes_firmware_properties_t *pProperties) {
    return L0::Firmware::fromHandle(hFirmware)->firmwareGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFirmwareFlash(
    zes_firmware_handle_t hFirmware,
    void *pImage,
    uint32_t size) {
    return L0::Firmware::fromHandle(hFirmware)->firmwareFlash(pImage, size);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumMemoryModules(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_mem_handle_t *phMemory) {
    return L0::SysmanDevice::memoryGet(hDevice, pCount, phMemory);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesMemoryGetProperties(
    zes_mem_handle_t hMemory,
    zes_mem_properties_t *pProperties) {
    return L0::Memory::fromHandle(hMemory)->memoryGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesMemoryGetState(
    zes_mem_handle_t hMemory,
    zes_mem_state_t *pState) {
    return L0::Memory::fromHandle(hMemory)->memoryGetState(pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesMemoryGetBandwidth(
    zes_mem_handle_t hMemory,
    zes_mem_bandwidth_t *pBandwidth) {
    return L0::Memory::fromHandle(hMemory)->memoryGetBandwidth(pBandwidth);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumFabricPorts(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_fabric_port_handle_t *phPort) {
    return L0::SysmanDevice::fabricPortGet(hDevice, pCount, phPort);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFabricPortGetProperties(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_properties_t *pProperties) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFabricPortGetLinkType(
    zes_fabric_port_handle_t hPort,
    zes_fabric_link_type_t *pLinkType) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetLinkType(pLinkType);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFabricPortGetConfig(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_config_t *pConfig) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetConfig(pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFabricPortSetConfig(
    zes_fabric_port_handle_t hPort,
    const zes_fabric_port_config_t *pConfig) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortSetConfig(pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFabricPortGetState(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_state_t *pState) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetState(pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFabricPortGetThroughput(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_throughput_t *pThroughput) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetThroughput(pThroughput);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumTemperatureSensors(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_temp_handle_t *phTemperature) {
    return L0::SysmanDevice::temperatureGet(hDevice, pCount, phTemperature);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesTemperatureGetProperties(
    zes_temp_handle_t hTemperature,
    zes_temp_properties_t *pProperties) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesTemperatureGetConfig(
    zes_temp_handle_t hTemperature,
    zes_temp_config_t *pConfig) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureGetConfig(pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesTemperatureSetConfig(
    zes_temp_handle_t hTemperature,
    const zes_temp_config_t *pConfig) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureSetConfig(pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesTemperatureGetState(
    zes_temp_handle_t hTemperature,
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

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPsuGetProperties(
    zes_psu_handle_t hPsu,
    zes_psu_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPsuGetState(
    zes_psu_handle_t hPsu,
    zes_psu_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumFans(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_fan_handle_t *phFan) {
    return L0::SysmanDevice::fanGet(hDevice, pCount, phFan);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFanGetProperties(
    zes_fan_handle_t hFan,
    zes_fan_properties_t *pProperties) {
    return L0::Fan::fromHandle(hFan)->fanGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFanGetConfig(
    zes_fan_handle_t hFan,
    zes_fan_config_t *pConfig) {
    return L0::Fan::fromHandle(hFan)->fanGetConfig(pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFanSetDefaultMode(
    zes_fan_handle_t hFan) {
    return L0::Fan::fromHandle(hFan)->fanSetDefaultMode();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFanSetFixedSpeedMode(
    zes_fan_handle_t hFan,
    const zes_fan_speed_t *speed) {
    return L0::Fan::fromHandle(hFan)->fanSetFixedSpeedMode(speed);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFanSetSpeedTableMode(
    zes_fan_handle_t hFan,
    const zes_fan_speed_table_t *speedTable) {
    return L0::Fan::fromHandle(hFan)->fanSetSpeedTableMode(speedTable);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesFanGetState(
    zes_fan_handle_t hFan,
    zes_fan_speed_units_t units,
    int32_t *pSpeed) {
    return L0::Fan::fromHandle(hFan)->fanGetState(units, pSpeed);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumLeds(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_led_handle_t *phLed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesLedGetProperties(
    zes_led_handle_t hLed,
    zes_led_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesLedGetState(
    zes_led_handle_t hLed,
    zes_led_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesLedSetState(
    zes_led_handle_t hLed,
    ze_bool_t enable) {
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
    return L0::SysmanDevice::rasGet(hDevice, pCount, phRas);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesRasGetProperties(
    zes_ras_handle_t hRas,
    zes_ras_properties_t *pProperties) {
    return L0::Ras::fromHandle(hRas)->rasGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesRasGetConfig(
    zes_ras_handle_t hRas,
    zes_ras_config_t *pConfig) {
    return L0::Ras::fromHandle(hRas)->rasGetConfig(pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesRasSetConfig(
    zes_ras_handle_t hRas,
    const zes_ras_config_t *pConfig) {
    return L0::Ras::fromHandle(hRas)->rasSetConfig(pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesRasGetState(
    zes_ras_handle_t hRas,
    ze_bool_t clear,
    zes_ras_state_t *pState) {
    return L0::Ras::fromHandle(hRas)->rasGetState(pState, clear);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEventRegister(
    zes_device_handle_t hDevice,
    zes_event_type_flags_t events) {
    return L0::SysmanDevice::deviceEventRegister(hDevice, events);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDriverEventListen(
    ze_driver_handle_t hDriver,
    uint32_t timeout,
    uint32_t count,
    zes_device_handle_t *phDevices,
    uint32_t *pNumDeviceEvents,
    zes_event_type_flags_t *pEvents) {
    return L0::DriverHandle::fromHandle(hDriver)->sysmanEventsListen(timeout, count, phDevices, pNumDeviceEvents, pEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDriverEventListenEx(
    ze_driver_handle_t hDriver,
    uint64_t timeout,
    uint32_t count,
    zes_device_handle_t *phDevices,
    uint32_t *pNumDeviceEvents,
    zes_event_type_flags_t *pEvents) {
    return L0::DriverHandle::fromHandle(hDriver)->sysmanEventsListenEx(timeout, count, phDevices, pNumDeviceEvents, pEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumDiagnosticTestSuites(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_diag_handle_t *phDiagnostics) {
    return L0::SysmanDevice::diagnosticsGet(hDevice, pCount, phDiagnostics);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDiagnosticsGetProperties(
    zes_diag_handle_t hDiagnostics,
    zes_diag_properties_t *pProperties) {
    return L0::Diagnostics::fromHandle(hDiagnostics)->diagnosticsGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDiagnosticsGetTests(
    zes_diag_handle_t hDiagnostics,
    uint32_t *pCount,
    zes_diag_test_t *pTests) {
    return L0::Diagnostics::fromHandle(hDiagnostics)->diagnosticsGetTests(pCount, pTests);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDiagnosticsRunTests(
    zes_diag_handle_t hDiagnostics,
    uint32_t startIndex,
    uint32_t endIndex,
    zes_diag_result_t *pResult) {
    return L0::Diagnostics::fromHandle(hDiagnostics)->diagnosticsRunTests(startIndex, endIndex, pResult);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumPerformanceFactorDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_perf_handle_t *phPerf) {
    return L0::SysmanDevice::performanceGet(hDevice, pCount, phPerf);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPerformanceFactorGetProperties(
    zes_perf_handle_t hPerf,
    zes_perf_properties_t *pProperties) {
    return L0::Performance::fromHandle(hPerf)->performanceGetProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPerformanceFactorGetConfig(
    zes_perf_handle_t hPerf,
    double *pFactor) {
    return L0::Performance::fromHandle(hPerf)->performanceGetConfig(pFactor);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zesPerformanceFactorSetConfig(
    zes_perf_handle_t hPerf,
    double factor) {
    return L0::Performance::fromHandle(hPerf)->performanceSetConfig(factor);
}
