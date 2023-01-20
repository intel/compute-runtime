/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/driver/driver_handle.h"

#include "sysman/sysman.h"

namespace L0 {
ze_result_t zesDeviceGetProperties(
    zes_device_handle_t hDevice,
    zes_device_properties_t *pProperties) {
    return L0::SysmanDevice::deviceGetProperties(hDevice, pProperties);
}

ze_result_t zesDeviceGetState(
    zes_device_handle_t hDevice,
    zes_device_state_t *pState) {
    return L0::SysmanDevice::deviceGetState(hDevice, pState);
}

ze_result_t zesDeviceEnumSchedulers(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_sched_handle_t *phScheduler) {
    return L0::SysmanDevice::schedulerGet(hDevice, pCount, phScheduler);
}

ze_result_t zesSchedulerGetProperties(
    zes_sched_handle_t hScheduler,
    zes_sched_properties_t *pProperties) {
    return L0::Scheduler::fromHandle(hScheduler)->schedulerGetProperties(pProperties);
}

ze_result_t zesSchedulerGetCurrentMode(
    zes_sched_handle_t hScheduler,
    zes_sched_mode_t *pMode) {
    return L0::Scheduler::fromHandle(hScheduler)->getCurrentMode(pMode);
}

ze_result_t zesSchedulerGetTimeoutModeProperties(
    zes_sched_handle_t hScheduler,
    ze_bool_t getDefaults,
    zes_sched_timeout_properties_t *pConfig) {
    return L0::Scheduler::fromHandle(hScheduler)->getTimeoutModeProperties(getDefaults, pConfig);
}

ze_result_t zesSchedulerGetTimesliceModeProperties(
    zes_sched_handle_t hScheduler,
    ze_bool_t getDefaults,
    zes_sched_timeslice_properties_t *pConfig) {
    return L0::Scheduler::fromHandle(hScheduler)->getTimesliceModeProperties(getDefaults, pConfig);
}

ze_result_t zesSchedulerSetTimeoutMode(
    zes_sched_handle_t hScheduler,
    zes_sched_timeout_properties_t *pProperties,
    ze_bool_t *pNeedReload) {
    return L0::Scheduler::fromHandle(hScheduler)->setTimeoutMode(pProperties, pNeedReload);
}

ze_result_t zesSchedulerSetTimesliceMode(
    zes_sched_handle_t hScheduler,
    zes_sched_timeslice_properties_t *pProperties,
    ze_bool_t *pNeedReload) {
    return L0::Scheduler::fromHandle(hScheduler)->setTimesliceMode(pProperties, pNeedReload);
}

ze_result_t zesSchedulerSetExclusiveMode(
    zes_sched_handle_t hScheduler,
    ze_bool_t *pNeedReload) {
    return L0::Scheduler::fromHandle(hScheduler)->setExclusiveMode(pNeedReload);
}

ze_result_t zesSchedulerSetComputeUnitDebugMode(
    zes_sched_handle_t hScheduler,
    ze_bool_t *pNeedReload) {
    return L0::Scheduler::fromHandle(hScheduler)->setComputeUnitDebugMode(pNeedReload);
}

ze_result_t zesDeviceProcessesGetState(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_process_state_t *pProcesses) {
    return L0::SysmanDevice::processesGetState(hDevice, pCount, pProcesses);
}

ze_result_t zesDeviceReset(
    zes_device_handle_t hDevice,
    ze_bool_t force) {
    return L0::SysmanDevice::deviceReset(hDevice, force);
}

ze_result_t zesDevicePciGetProperties(
    zes_device_handle_t hDevice,
    zes_pci_properties_t *pProperties) {
    return L0::SysmanDevice::pciGetProperties(hDevice, pProperties);
}

ze_result_t zesDevicePciGetState(
    zes_device_handle_t hDevice,
    zes_pci_state_t *pState) {
    return L0::SysmanDevice::pciGetState(hDevice, pState);
}

ze_result_t zesDevicePciGetBars(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_pci_bar_properties_t *pProperties) {
    return L0::SysmanDevice::pciGetBars(hDevice, pCount, pProperties);
}

ze_result_t zesDevicePciGetStats(
    zes_device_handle_t hDevice,
    zes_pci_stats_t *pStats) {
    return L0::SysmanDevice::pciGetStats(hDevice, pStats);
}

ze_result_t zesDeviceEnumPowerDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_pwr_handle_t *phPower) {
    return L0::SysmanDevice::powerGet(hDevice, pCount, phPower);
}

ze_result_t zesDeviceGetCardPowerDomain(
    zes_device_handle_t hDevice,
    zes_pwr_handle_t *phPower) {
    return L0::SysmanDevice::powerGetCardDomain(hDevice, phPower);
}

ze_result_t zesPowerGetProperties(
    zes_pwr_handle_t hPower,
    zes_power_properties_t *pProperties) {
    return L0::Power::fromHandle(hPower)->powerGetProperties(pProperties);
}

ze_result_t zesPowerGetEnergyCounter(
    zes_pwr_handle_t hPower,
    zes_power_energy_counter_t *pEnergy) {
    return L0::Power::fromHandle(hPower)->powerGetEnergyCounter(pEnergy);
}

ze_result_t zesPowerGetLimits(
    zes_pwr_handle_t hPower,
    zes_power_sustained_limit_t *pSustained,
    zes_power_burst_limit_t *pBurst,
    zes_power_peak_limit_t *pPeak) {
    return L0::Power::fromHandle(hPower)->powerGetLimits(pSustained, pBurst, pPeak);
}

ze_result_t zesPowerSetLimits(
    zes_pwr_handle_t hPower,
    const zes_power_sustained_limit_t *pSustained,
    const zes_power_burst_limit_t *pBurst,
    const zes_power_peak_limit_t *pPeak) {
    return L0::Power::fromHandle(hPower)->powerSetLimits(pSustained, pBurst, pPeak);
}

ze_result_t zesPowerGetLimitsExt(
    zes_pwr_handle_t hPower,
    uint32_t *pCount,
    zes_power_limit_ext_desc_t *pSustained) {
    return L0::Power::fromHandle(hPower)->powerGetLimitsExt(pCount, pSustained);
}

ze_result_t zesPowerSetLimitsExt(
    zes_pwr_handle_t hPower,
    uint32_t *pCount,
    zes_power_limit_ext_desc_t *pSustained) {
    return L0::Power::fromHandle(hPower)->powerSetLimitsExt(pCount, pSustained);
}

ze_result_t zesPowerGetEnergyThreshold(
    zes_pwr_handle_t hPower,
    zes_energy_threshold_t *pThreshold) {
    return L0::Power::fromHandle(hPower)->powerGetEnergyThreshold(pThreshold);
}

ze_result_t zesPowerSetEnergyThreshold(
    zes_pwr_handle_t hPower,
    double threshold) {
    return L0::Power::fromHandle(hPower)->powerSetEnergyThreshold(threshold);
}

ze_result_t zesDeviceEnumFrequencyDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_freq_handle_t *phFrequency) {
    return L0::SysmanDevice::frequencyGet(hDevice, pCount, phFrequency);
}

ze_result_t zesFrequencyGetProperties(
    zes_freq_handle_t hFrequency,
    zes_freq_properties_t *pProperties) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetProperties(pProperties);
}

ze_result_t zesFrequencyGetAvailableClocks(
    zes_freq_handle_t hFrequency,
    uint32_t *pCount,
    double *phFrequency) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetAvailableClocks(pCount, phFrequency);
}

ze_result_t zesFrequencyGetRange(
    zes_freq_handle_t hFrequency,
    zes_freq_range_t *pLimits) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetRange(pLimits);
}

ze_result_t zesFrequencySetRange(
    zes_freq_handle_t hFrequency,
    const zes_freq_range_t *pLimits) {
    return L0::Frequency::fromHandle(hFrequency)->frequencySetRange(pLimits);
}

ze_result_t zesFrequencyGetState(
    zes_freq_handle_t hFrequency,
    zes_freq_state_t *pState) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetState(pState);
}

ze_result_t zesFrequencyGetThrottleTime(
    zes_freq_handle_t hFrequency,
    zes_freq_throttle_time_t *pThrottleTime) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyGetThrottleTime(pThrottleTime);
}

ze_result_t zesFrequencyOcGetFrequencyTarget(
    zes_freq_handle_t hFrequency,
    double *pCurrentOcFrequency) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcGetFrequencyTarget(pCurrentOcFrequency);
}

ze_result_t zesFrequencyOcSetFrequencyTarget(
    zes_freq_handle_t hFrequency,
    double currentOcFrequency) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcSetFrequencyTarget(currentOcFrequency);
}

ze_result_t zesFrequencyOcGetVoltageTarget(
    zes_freq_handle_t hFrequency,
    double *pCurrentVoltageTarget,
    double *pCurrentVoltageOffset) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcGetVoltageTarget(pCurrentVoltageTarget, pCurrentVoltageOffset);
}

ze_result_t zesFrequencyOcSetVoltageTarget(
    zes_freq_handle_t hFrequency,
    double currentVoltageTarget,
    double currentVoltageOffset) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcSetVoltageTarget(currentVoltageTarget, currentVoltageOffset);
}

ze_result_t zesFrequencyOcSetMode(
    zes_freq_handle_t hFrequency,
    zes_oc_mode_t currentOcMode) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcSetMode(currentOcMode);
}

ze_result_t zesFrequencyOcGetMode(
    zes_freq_handle_t hFrequency,
    zes_oc_mode_t *pCurrentOcMode) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcGetMode(pCurrentOcMode);
}

ze_result_t zesFrequencyOcGetCapabilities(
    zes_freq_handle_t hFrequency,
    zes_oc_capabilities_t *pOcCapabilities) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcGetCapabilities(pOcCapabilities);
}

ze_result_t zesFrequencyOcGetIccMax(
    zes_freq_handle_t hFrequency,
    double *pOcIccMax) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcGetIccMax(pOcIccMax);
}

ze_result_t zesFrequencyOcSetIccMax(
    zes_freq_handle_t hFrequency,
    double ocIccMax) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcSetIccMax(ocIccMax);
}

ze_result_t zesFrequencyOcGetTjMax(
    zes_freq_handle_t hFrequency,
    double *pOcTjMax) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcGeTjMax(pOcTjMax);
}

ze_result_t zesFrequencyOcSetTjMax(
    zes_freq_handle_t hFrequency,
    double ocTjMax) {
    return L0::Frequency::fromHandle(hFrequency)->frequencyOcSetTjMax(ocTjMax);
}

ze_result_t zesDeviceEnumEngineGroups(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_engine_handle_t *phEngine) {
    return L0::SysmanDevice::engineGet(hDevice, pCount, phEngine);
}

ze_result_t zesEngineGetProperties(
    zes_engine_handle_t hEngine,
    zes_engine_properties_t *pProperties) {
    return L0::Engine::fromHandle(hEngine)->engineGetProperties(pProperties);
}

ze_result_t zesEngineGetActivity(
    zes_engine_handle_t hEngine,
    zes_engine_stats_t *pStats) {
    return L0::Engine::fromHandle(hEngine)->engineGetActivity(pStats);
}

ze_result_t zesDeviceEnumStandbyDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_standby_handle_t *phStandby) {
    return L0::SysmanDevice::standbyGet(hDevice, pCount, phStandby);
}

ze_result_t zesStandbyGetProperties(
    zes_standby_handle_t hStandby,
    zes_standby_properties_t *pProperties) {
    return L0::Standby::fromHandle(hStandby)->standbyGetProperties(pProperties);
}

ze_result_t zesStandbyGetMode(
    zes_standby_handle_t hStandby,
    zes_standby_promo_mode_t *pMode) {
    return L0::Standby::fromHandle(hStandby)->standbyGetMode(pMode);
}

ze_result_t zesStandbySetMode(
    zes_standby_handle_t hStandby,
    zes_standby_promo_mode_t mode) {
    return L0::Standby::fromHandle(hStandby)->standbySetMode(mode);
}

ze_result_t zesDeviceEnumFirmwares(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_firmware_handle_t *phFirmware) {
    return L0::SysmanDevice::firmwareGet(hDevice, pCount, phFirmware);
}

ze_result_t zesFirmwareGetProperties(
    zes_firmware_handle_t hFirmware,
    zes_firmware_properties_t *pProperties) {
    return L0::Firmware::fromHandle(hFirmware)->firmwareGetProperties(pProperties);
}

ze_result_t zesFirmwareFlash(
    zes_firmware_handle_t hFirmware,
    void *pImage,
    uint32_t size) {
    return L0::Firmware::fromHandle(hFirmware)->firmwareFlash(pImage, size);
}

ze_result_t zesDeviceEnumMemoryModules(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_mem_handle_t *phMemory) {
    return L0::SysmanDevice::memoryGet(hDevice, pCount, phMemory);
}

ze_result_t zesMemoryGetProperties(
    zes_mem_handle_t hMemory,
    zes_mem_properties_t *pProperties) {
    return L0::Memory::fromHandle(hMemory)->memoryGetProperties(pProperties);
}

ze_result_t zesMemoryGetState(
    zes_mem_handle_t hMemory,
    zes_mem_state_t *pState) {
    return L0::Memory::fromHandle(hMemory)->memoryGetState(pState);
}

ze_result_t zesMemoryGetBandwidth(
    zes_mem_handle_t hMemory,
    zes_mem_bandwidth_t *pBandwidth) {
    return L0::Memory::fromHandle(hMemory)->memoryGetBandwidth(pBandwidth);
}

ze_result_t zesDeviceEnumFabricPorts(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_fabric_port_handle_t *phPort) {
    return L0::SysmanDevice::fabricPortGet(hDevice, pCount, phPort);
}

ze_result_t zesFabricPortGetProperties(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_properties_t *pProperties) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetProperties(pProperties);
}

ze_result_t zesFabricPortGetLinkType(
    zes_fabric_port_handle_t hPort,
    zes_fabric_link_type_t *pLinkType) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetLinkType(pLinkType);
}

ze_result_t zesFabricPortGetConfig(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_config_t *pConfig) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetConfig(pConfig);
}

ze_result_t zesFabricPortSetConfig(
    zes_fabric_port_handle_t hPort,
    const zes_fabric_port_config_t *pConfig) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortSetConfig(pConfig);
}

ze_result_t zesFabricPortGetState(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_state_t *pState) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetState(pState);
}

ze_result_t zesFabricPortGetThroughput(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_throughput_t *pThroughput) {
    return L0::FabricPort::fromHandle(hPort)->fabricPortGetThroughput(pThroughput);
}

ze_result_t zesDeviceEnumTemperatureSensors(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_temp_handle_t *phTemperature) {
    return L0::SysmanDevice::temperatureGet(hDevice, pCount, phTemperature);
}

ze_result_t zesTemperatureGetProperties(
    zes_temp_handle_t hTemperature,
    zes_temp_properties_t *pProperties) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureGetProperties(pProperties);
}

ze_result_t zesTemperatureGetConfig(
    zes_temp_handle_t hTemperature,
    zes_temp_config_t *pConfig) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureGetConfig(pConfig);
}

ze_result_t zesTemperatureSetConfig(
    zes_temp_handle_t hTemperature,
    const zes_temp_config_t *pConfig) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureSetConfig(pConfig);
}

ze_result_t zesTemperatureGetState(
    zes_temp_handle_t hTemperature,
    double *pTemperature) {
    return L0::Temperature::fromHandle(hTemperature)->temperatureGetState(pTemperature);
}

ze_result_t zesDeviceEnumPsus(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_psu_handle_t *phPsu) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zesPsuGetProperties(
    zes_psu_handle_t hPsu,
    zes_psu_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zesPsuGetState(
    zes_psu_handle_t hPsu,
    zes_psu_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zesDeviceEnumFans(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_fan_handle_t *phFan) {
    return L0::SysmanDevice::fanGet(hDevice, pCount, phFan);
}

ze_result_t zesFanGetProperties(
    zes_fan_handle_t hFan,
    zes_fan_properties_t *pProperties) {
    return L0::Fan::fromHandle(hFan)->fanGetProperties(pProperties);
}

ze_result_t zesFanGetConfig(
    zes_fan_handle_t hFan,
    zes_fan_config_t *pConfig) {
    return L0::Fan::fromHandle(hFan)->fanGetConfig(pConfig);
}

ze_result_t zesFanSetDefaultMode(
    zes_fan_handle_t hFan) {
    return L0::Fan::fromHandle(hFan)->fanSetDefaultMode();
}

ze_result_t zesFanSetFixedSpeedMode(
    zes_fan_handle_t hFan,
    const zes_fan_speed_t *speed) {
    return L0::Fan::fromHandle(hFan)->fanSetFixedSpeedMode(speed);
}

ze_result_t zesFanSetSpeedTableMode(
    zes_fan_handle_t hFan,
    const zes_fan_speed_table_t *speedTable) {
    return L0::Fan::fromHandle(hFan)->fanSetSpeedTableMode(speedTable);
}

ze_result_t zesFanGetState(
    zes_fan_handle_t hFan,
    zes_fan_speed_units_t units,
    int32_t *pSpeed) {
    return L0::Fan::fromHandle(hFan)->fanGetState(units, pSpeed);
}

ze_result_t zesDeviceEnumLeds(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_led_handle_t *phLed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zesLedGetProperties(
    zes_led_handle_t hLed,
    zes_led_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zesLedGetState(
    zes_led_handle_t hLed,
    zes_led_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zesLedSetState(
    zes_led_handle_t hLed,
    ze_bool_t enable) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zesLedSetColor(
    zes_led_handle_t hLed,
    const zes_led_color_t *pColor) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zesDeviceEnumRasErrorSets(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_ras_handle_t *phRas) {
    return L0::SysmanDevice::rasGet(hDevice, pCount, phRas);
}

ze_result_t zesRasGetProperties(
    zes_ras_handle_t hRas,
    zes_ras_properties_t *pProperties) {
    return L0::Ras::fromHandle(hRas)->rasGetProperties(pProperties);
}

ze_result_t zesRasGetConfig(
    zes_ras_handle_t hRas,
    zes_ras_config_t *pConfig) {
    return L0::Ras::fromHandle(hRas)->rasGetConfig(pConfig);
}

ze_result_t zesRasSetConfig(
    zes_ras_handle_t hRas,
    const zes_ras_config_t *pConfig) {
    return L0::Ras::fromHandle(hRas)->rasSetConfig(pConfig);
}

ze_result_t zesRasGetState(
    zes_ras_handle_t hRas,
    ze_bool_t clear,
    zes_ras_state_t *pState) {
    return L0::Ras::fromHandle(hRas)->rasGetState(pState, clear);
}

ze_result_t zesDeviceEventRegister(
    zes_device_handle_t hDevice,
    zes_event_type_flags_t events) {
    return L0::SysmanDevice::deviceEventRegister(hDevice, events);
}

ze_result_t zesDriverEventListen(
    ze_driver_handle_t hDriver,
    uint32_t timeout,
    uint32_t count,
    zes_device_handle_t *phDevices,
    uint32_t *pNumDeviceEvents,
    zes_event_type_flags_t *pEvents) {
    return L0::DriverHandle::fromHandle(hDriver)->sysmanEventsListen(timeout, count, phDevices, pNumDeviceEvents, pEvents);
}

ze_result_t zesDriverEventListenEx(
    ze_driver_handle_t hDriver,
    uint64_t timeout,
    uint32_t count,
    zes_device_handle_t *phDevices,
    uint32_t *pNumDeviceEvents,
    zes_event_type_flags_t *pEvents) {
    return L0::DriverHandle::fromHandle(hDriver)->sysmanEventsListenEx(timeout, count, phDevices, pNumDeviceEvents, pEvents);
}

ze_result_t zesDeviceEnumDiagnosticTestSuites(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_diag_handle_t *phDiagnostics) {
    return L0::SysmanDevice::diagnosticsGet(hDevice, pCount, phDiagnostics);
}

ze_result_t zesDiagnosticsGetProperties(
    zes_diag_handle_t hDiagnostics,
    zes_diag_properties_t *pProperties) {
    return L0::Diagnostics::fromHandle(hDiagnostics)->diagnosticsGetProperties(pProperties);
}

ze_result_t zesDiagnosticsGetTests(
    zes_diag_handle_t hDiagnostics,
    uint32_t *pCount,
    zes_diag_test_t *pTests) {
    return L0::Diagnostics::fromHandle(hDiagnostics)->diagnosticsGetTests(pCount, pTests);
}

ze_result_t zesDiagnosticsRunTests(
    zes_diag_handle_t hDiagnostics,
    uint32_t startIndex,
    uint32_t endIndex,
    zes_diag_result_t *pResult) {
    return L0::Diagnostics::fromHandle(hDiagnostics)->diagnosticsRunTests(startIndex, endIndex, pResult);
}

ze_result_t zesDeviceEnumPerformanceFactorDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_perf_handle_t *phPerf) {
    return L0::SysmanDevice::performanceGet(hDevice, pCount, phPerf);
}

ze_result_t zesPerformanceFactorGetProperties(
    zes_perf_handle_t hPerf,
    zes_perf_properties_t *pProperties) {
    return L0::Performance::fromHandle(hPerf)->performanceGetProperties(pProperties);
}

ze_result_t zesPerformanceFactorGetConfig(
    zes_perf_handle_t hPerf,
    double *pFactor) {
    return L0::Performance::fromHandle(hPerf)->performanceGetConfig(pFactor);
}

ze_result_t zesPerformanceFactorSetConfig(
    zes_perf_handle_t hPerf,
    double factor) {
    return L0::Performance::fromHandle(hPerf)->performanceSetConfig(factor);
}

ze_result_t zesDeviceEccAvailable(
    zes_device_handle_t hDevice,
    ze_bool_t *pAvailable) {
    return L0::SysmanDevice::deviceEccAvailable(hDevice, pAvailable);
}

ze_result_t zesDeviceEccConfigurable(
    zes_device_handle_t hDevice,
    ze_bool_t *pConfigurable) {
    return L0::SysmanDevice::deviceEccConfigurable(hDevice, pConfigurable);
}

ze_result_t zesDeviceGetEccState(
    zes_device_handle_t hDevice,
    zes_device_ecc_properties_t *pState) {
    return L0::SysmanDevice::deviceGetEccState(hDevice, pState);
}

ze_result_t zesDeviceSetEccState(
    zes_device_handle_t hDevice,
    const zes_device_ecc_desc_t *newState,
    zes_device_ecc_properties_t *pState) {
    return L0::SysmanDevice::deviceSetEccState(hDevice, newState, pState);
}

} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceGetProperties(
    zes_device_handle_t hDevice,
    zes_device_properties_t *pProperties) {
    return L0::zesDeviceGetProperties(
        hDevice,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceGetState(
    zes_device_handle_t hDevice,
    zes_device_state_t *pState) {
    return L0::zesDeviceGetState(
        hDevice,
        pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceReset(
    zes_device_handle_t hDevice,
    ze_bool_t force) {
    return L0::zesDeviceReset(
        hDevice,
        force);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceProcessesGetState(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_process_state_t *pProcesses) {
    return L0::zesDeviceProcessesGetState(
        hDevice,
        pCount,
        pProcesses);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDevicePciGetProperties(
    zes_device_handle_t hDevice,
    zes_pci_properties_t *pProperties) {
    return L0::zesDevicePciGetProperties(
        hDevice,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDevicePciGetState(
    zes_device_handle_t hDevice,
    zes_pci_state_t *pState) {
    return L0::zesDevicePciGetState(
        hDevice,
        pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDevicePciGetBars(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_pci_bar_properties_t *pProperties) {
    return L0::zesDevicePciGetBars(
        hDevice,
        pCount,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDevicePciGetStats(
    zes_device_handle_t hDevice,
    zes_pci_stats_t *pStats) {
    return L0::zesDevicePciGetStats(
        hDevice,
        pStats);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumDiagnosticTestSuites(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_diag_handle_t *phDiagnostics) {
    return L0::zesDeviceEnumDiagnosticTestSuites(
        hDevice,
        pCount,
        phDiagnostics);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDiagnosticsGetProperties(
    zes_diag_handle_t hDiagnostics,
    zes_diag_properties_t *pProperties) {
    return L0::zesDiagnosticsGetProperties(
        hDiagnostics,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDiagnosticsGetTests(
    zes_diag_handle_t hDiagnostics,
    uint32_t *pCount,
    zes_diag_test_t *pTests) {
    return L0::zesDiagnosticsGetTests(
        hDiagnostics,
        pCount,
        pTests);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDiagnosticsRunTests(
    zes_diag_handle_t hDiagnostics,
    uint32_t startIndex,
    uint32_t endIndex,
    zes_diag_result_t *pResult) {
    return L0::zesDiagnosticsRunTests(
        hDiagnostics,
        startIndex,
        endIndex,
        pResult);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEccAvailable(
    zes_device_handle_t hDevice,
    ze_bool_t *pAvailable) {
    return L0::zesDeviceEccAvailable(
        hDevice,
        pAvailable);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEccConfigurable(
    zes_device_handle_t hDevice,
    ze_bool_t *pConfigurable) {
    return L0::zesDeviceEccConfigurable(
        hDevice,
        pConfigurable);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceGetEccState(
    zes_device_handle_t hDevice,
    zes_device_ecc_properties_t *pState) {
    return L0::zesDeviceGetEccState(
        hDevice,
        pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceSetEccState(
    zes_device_handle_t hDevice,
    const zes_device_ecc_desc_t *newState,
    zes_device_ecc_properties_t *pState) {
    return L0::zesDeviceSetEccState(
        hDevice,
        newState,
        pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumEngineGroups(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_engine_handle_t *phEngine) {
    return L0::zesDeviceEnumEngineGroups(
        hDevice,
        pCount,
        phEngine);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesEngineGetProperties(
    zes_engine_handle_t hEngine,
    zes_engine_properties_t *pProperties) {
    return L0::zesEngineGetProperties(
        hEngine,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesEngineGetActivity(
    zes_engine_handle_t hEngine,
    zes_engine_stats_t *pStats) {
    return L0::zesEngineGetActivity(
        hEngine,
        pStats);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEventRegister(
    zes_device_handle_t hDevice,
    zes_event_type_flags_t events) {
    return L0::zesDeviceEventRegister(
        hDevice,
        events);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDriverEventListen(
    ze_driver_handle_t hDriver,
    uint32_t timeout,
    uint32_t count,
    zes_device_handle_t *phDevices,
    uint32_t *pNumDeviceEvents,
    zes_event_type_flags_t *pEvents) {
    return L0::zesDriverEventListen(
        hDriver,
        timeout,
        count,
        phDevices,
        pNumDeviceEvents,
        pEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDriverEventListenEx(
    ze_driver_handle_t hDriver,
    uint64_t timeout,
    uint32_t count,
    zes_device_handle_t *phDevices,
    uint32_t *pNumDeviceEvents,
    zes_event_type_flags_t *pEvents) {
    return L0::zesDriverEventListenEx(
        hDriver,
        timeout,
        count,
        phDevices,
        pNumDeviceEvents,
        pEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumFabricPorts(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_fabric_port_handle_t *phPort) {
    return L0::zesDeviceEnumFabricPorts(
        hDevice,
        pCount,
        phPort);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFabricPortGetProperties(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_properties_t *pProperties) {
    return L0::zesFabricPortGetProperties(
        hPort,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFabricPortGetLinkType(
    zes_fabric_port_handle_t hPort,
    zes_fabric_link_type_t *pLinkType) {
    return L0::zesFabricPortGetLinkType(
        hPort,
        pLinkType);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFabricPortGetConfig(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_config_t *pConfig) {
    return L0::zesFabricPortGetConfig(
        hPort,
        pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFabricPortSetConfig(
    zes_fabric_port_handle_t hPort,
    const zes_fabric_port_config_t *pConfig) {
    return L0::zesFabricPortSetConfig(
        hPort,
        pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFabricPortGetState(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_state_t *pState) {
    return L0::zesFabricPortGetState(
        hPort,
        pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFabricPortGetThroughput(
    zes_fabric_port_handle_t hPort,
    zes_fabric_port_throughput_t *pThroughput) {
    return L0::zesFabricPortGetThroughput(
        hPort,
        pThroughput);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumFans(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_fan_handle_t *phFan) {
    return L0::zesDeviceEnumFans(
        hDevice,
        pCount,
        phFan);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFanGetProperties(
    zes_fan_handle_t hFan,
    zes_fan_properties_t *pProperties) {
    return L0::zesFanGetProperties(
        hFan,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFanGetConfig(
    zes_fan_handle_t hFan,
    zes_fan_config_t *pConfig) {
    return L0::zesFanGetConfig(
        hFan,
        pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFanSetDefaultMode(
    zes_fan_handle_t hFan) {
    return L0::zesFanSetDefaultMode(
        hFan);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFanSetFixedSpeedMode(
    zes_fan_handle_t hFan,
    const zes_fan_speed_t *speed) {
    return L0::zesFanSetFixedSpeedMode(
        hFan,
        speed);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFanSetSpeedTableMode(
    zes_fan_handle_t hFan,
    const zes_fan_speed_table_t *speedTable) {
    return L0::zesFanSetSpeedTableMode(
        hFan,
        speedTable);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFanGetState(
    zes_fan_handle_t hFan,
    zes_fan_speed_units_t units,
    int32_t *pSpeed) {
    return L0::zesFanGetState(
        hFan,
        units,
        pSpeed);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumFirmwares(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_firmware_handle_t *phFirmware) {
    return L0::zesDeviceEnumFirmwares(
        hDevice,
        pCount,
        phFirmware);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFirmwareGetProperties(
    zes_firmware_handle_t hFirmware,
    zes_firmware_properties_t *pProperties) {
    return L0::zesFirmwareGetProperties(
        hFirmware,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFirmwareFlash(
    zes_firmware_handle_t hFirmware,
    void *pImage,
    uint32_t size) {
    return L0::zesFirmwareFlash(
        hFirmware,
        pImage,
        size);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumFrequencyDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_freq_handle_t *phFrequency) {
    return L0::zesDeviceEnumFrequencyDomains(
        hDevice,
        pCount,
        phFrequency);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyGetProperties(
    zes_freq_handle_t hFrequency,
    zes_freq_properties_t *pProperties) {
    return L0::zesFrequencyGetProperties(
        hFrequency,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyGetAvailableClocks(
    zes_freq_handle_t hFrequency,
    uint32_t *pCount,
    double *phFrequency) {
    return L0::zesFrequencyGetAvailableClocks(
        hFrequency,
        pCount,
        phFrequency);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyGetRange(
    zes_freq_handle_t hFrequency,
    zes_freq_range_t *pLimits) {
    return L0::zesFrequencyGetRange(
        hFrequency,
        pLimits);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencySetRange(
    zes_freq_handle_t hFrequency,
    const zes_freq_range_t *pLimits) {
    return L0::zesFrequencySetRange(
        hFrequency,
        pLimits);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyGetState(
    zes_freq_handle_t hFrequency,
    zes_freq_state_t *pState) {
    return L0::zesFrequencyGetState(
        hFrequency,
        pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyGetThrottleTime(
    zes_freq_handle_t hFrequency,
    zes_freq_throttle_time_t *pThrottleTime) {
    return L0::zesFrequencyGetThrottleTime(
        hFrequency,
        pThrottleTime);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyOcGetCapabilities(
    zes_freq_handle_t hFrequency,
    zes_oc_capabilities_t *pOcCapabilities) {
    return L0::zesFrequencyOcGetCapabilities(
        hFrequency,
        pOcCapabilities);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyOcGetFrequencyTarget(
    zes_freq_handle_t hFrequency,
    double *pCurrentOcFrequency) {
    return L0::zesFrequencyOcGetFrequencyTarget(
        hFrequency,
        pCurrentOcFrequency);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyOcSetFrequencyTarget(
    zes_freq_handle_t hFrequency,
    double currentOcFrequency) {
    return L0::zesFrequencyOcSetFrequencyTarget(
        hFrequency,
        currentOcFrequency);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyOcGetVoltageTarget(
    zes_freq_handle_t hFrequency,
    double *pCurrentVoltageTarget,
    double *pCurrentVoltageOffset) {
    return L0::zesFrequencyOcGetVoltageTarget(
        hFrequency,
        pCurrentVoltageTarget,
        pCurrentVoltageOffset);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyOcSetVoltageTarget(
    zes_freq_handle_t hFrequency,
    double currentVoltageTarget,
    double currentVoltageOffset) {
    return L0::zesFrequencyOcSetVoltageTarget(
        hFrequency,
        currentVoltageTarget,
        currentVoltageOffset);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyOcSetMode(
    zes_freq_handle_t hFrequency,
    zes_oc_mode_t currentOcMode) {
    return L0::zesFrequencyOcSetMode(
        hFrequency,
        currentOcMode);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyOcGetMode(
    zes_freq_handle_t hFrequency,
    zes_oc_mode_t *pCurrentOcMode) {
    return L0::zesFrequencyOcGetMode(
        hFrequency,
        pCurrentOcMode);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyOcGetIccMax(
    zes_freq_handle_t hFrequency,
    double *pOcIccMax) {
    return L0::zesFrequencyOcGetIccMax(
        hFrequency,
        pOcIccMax);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyOcSetIccMax(
    zes_freq_handle_t hFrequency,
    double ocIccMax) {
    return L0::zesFrequencyOcSetIccMax(
        hFrequency,
        ocIccMax);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyOcGetTjMax(
    zes_freq_handle_t hFrequency,
    double *pOcTjMax) {
    return L0::zesFrequencyOcGetTjMax(
        hFrequency,
        pOcTjMax);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesFrequencyOcSetTjMax(
    zes_freq_handle_t hFrequency,
    double ocTjMax) {
    return L0::zesFrequencyOcSetTjMax(
        hFrequency,
        ocTjMax);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumLeds(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_led_handle_t *phLed) {
    return L0::zesDeviceEnumLeds(
        hDevice,
        pCount,
        phLed);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesLedGetProperties(
    zes_led_handle_t hLed,
    zes_led_properties_t *pProperties) {
    return L0::zesLedGetProperties(
        hLed,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesLedGetState(
    zes_led_handle_t hLed,
    zes_led_state_t *pState) {
    return L0::zesLedGetState(
        hLed,
        pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesLedSetState(
    zes_led_handle_t hLed,
    ze_bool_t enable) {
    return L0::zesLedSetState(
        hLed,
        enable);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesLedSetColor(
    zes_led_handle_t hLed,
    const zes_led_color_t *pColor) {
    return L0::zesLedSetColor(
        hLed,
        pColor);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumMemoryModules(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_mem_handle_t *phMemory) {
    return L0::zesDeviceEnumMemoryModules(
        hDevice,
        pCount,
        phMemory);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesMemoryGetProperties(
    zes_mem_handle_t hMemory,
    zes_mem_properties_t *pProperties) {
    return L0::zesMemoryGetProperties(
        hMemory,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesMemoryGetState(
    zes_mem_handle_t hMemory,
    zes_mem_state_t *pState) {
    return L0::zesMemoryGetState(
        hMemory,
        pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesMemoryGetBandwidth(
    zes_mem_handle_t hMemory,
    zes_mem_bandwidth_t *pBandwidth) {
    return L0::zesMemoryGetBandwidth(
        hMemory,
        pBandwidth);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumPerformanceFactorDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_perf_handle_t *phPerf) {
    return L0::zesDeviceEnumPerformanceFactorDomains(
        hDevice,
        pCount,
        phPerf);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesPerformanceFactorGetProperties(
    zes_perf_handle_t hPerf,
    zes_perf_properties_t *pProperties) {
    return L0::zesPerformanceFactorGetProperties(
        hPerf,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesPerformanceFactorGetConfig(
    zes_perf_handle_t hPerf,
    double *pFactor) {
    return L0::zesPerformanceFactorGetConfig(
        hPerf,
        pFactor);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesPerformanceFactorSetConfig(
    zes_perf_handle_t hPerf,
    double factor) {
    return L0::zesPerformanceFactorSetConfig(
        hPerf,
        factor);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumPowerDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_pwr_handle_t *phPower) {
    return L0::zesDeviceEnumPowerDomains(
        hDevice,
        pCount,
        phPower);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceGetCardPowerDomain(
    zes_device_handle_t hDevice,
    zes_pwr_handle_t *phPower) {
    return L0::zesDeviceGetCardPowerDomain(
        hDevice,
        phPower);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesPowerGetProperties(
    zes_pwr_handle_t hPower,
    zes_power_properties_t *pProperties) {
    return L0::zesPowerGetProperties(
        hPower,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesPowerGetEnergyCounter(
    zes_pwr_handle_t hPower,
    zes_power_energy_counter_t *pEnergy) {
    return L0::zesPowerGetEnergyCounter(
        hPower,
        pEnergy);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesPowerGetLimits(
    zes_pwr_handle_t hPower,
    zes_power_sustained_limit_t *pSustained,
    zes_power_burst_limit_t *pBurst,
    zes_power_peak_limit_t *pPeak) {
    return L0::zesPowerGetLimits(
        hPower,
        pSustained,
        pBurst,
        pPeak);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesPowerSetLimits(
    zes_pwr_handle_t hPower,
    const zes_power_sustained_limit_t *pSustained,
    const zes_power_burst_limit_t *pBurst,
    const zes_power_peak_limit_t *pPeak) {
    return L0::zesPowerSetLimits(
        hPower,
        pSustained,
        pBurst,
        pPeak);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesPowerGetLimitsExt(
    zes_pwr_handle_t hPower,
    uint32_t *pCount,
    zes_power_limit_ext_desc_t *pSustained) {
    return L0::zesPowerGetLimitsExt(
        hPower,
        pCount,
        pSustained);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesPowerSetLimitsExt(
    zes_pwr_handle_t hPower,
    uint32_t *pCount,
    zes_power_limit_ext_desc_t *pSustained) {
    return L0::zesPowerSetLimitsExt(
        hPower,
        pCount,
        pSustained);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesPowerGetEnergyThreshold(
    zes_pwr_handle_t hPower,
    zes_energy_threshold_t *pThreshold) {
    return L0::zesPowerGetEnergyThreshold(
        hPower,
        pThreshold);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesPowerSetEnergyThreshold(
    zes_pwr_handle_t hPower,
    double threshold) {
    return L0::zesPowerSetEnergyThreshold(
        hPower,
        threshold);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumPsus(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_psu_handle_t *phPsu) {
    return L0::zesDeviceEnumPsus(
        hDevice,
        pCount,
        phPsu);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesPsuGetProperties(
    zes_psu_handle_t hPsu,
    zes_psu_properties_t *pProperties) {
    return L0::zesPsuGetProperties(
        hPsu,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesPsuGetState(
    zes_psu_handle_t hPsu,
    zes_psu_state_t *pState) {
    return L0::zesPsuGetState(
        hPsu,
        pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumRasErrorSets(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_ras_handle_t *phRas) {
    return L0::zesDeviceEnumRasErrorSets(
        hDevice,
        pCount,
        phRas);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesRasGetProperties(
    zes_ras_handle_t hRas,
    zes_ras_properties_t *pProperties) {
    return L0::zesRasGetProperties(
        hRas,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesRasGetConfig(
    zes_ras_handle_t hRas,
    zes_ras_config_t *pConfig) {
    return L0::zesRasGetConfig(
        hRas,
        pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesRasSetConfig(
    zes_ras_handle_t hRas,
    const zes_ras_config_t *pConfig) {
    return L0::zesRasSetConfig(
        hRas,
        pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesRasGetState(
    zes_ras_handle_t hRas,
    ze_bool_t clear,
    zes_ras_state_t *pState) {
    return L0::zesRasGetState(
        hRas,
        clear,
        pState);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumSchedulers(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_sched_handle_t *phScheduler) {
    return L0::zesDeviceEnumSchedulers(
        hDevice,
        pCount,
        phScheduler);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesSchedulerGetProperties(
    zes_sched_handle_t hScheduler,
    zes_sched_properties_t *pProperties) {
    return L0::zesSchedulerGetProperties(
        hScheduler,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesSchedulerGetCurrentMode(
    zes_sched_handle_t hScheduler,
    zes_sched_mode_t *pMode) {
    return L0::zesSchedulerGetCurrentMode(
        hScheduler,
        pMode);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesSchedulerGetTimeoutModeProperties(
    zes_sched_handle_t hScheduler,
    ze_bool_t getDefaults,
    zes_sched_timeout_properties_t *pConfig) {
    return L0::zesSchedulerGetTimeoutModeProperties(
        hScheduler,
        getDefaults,
        pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesSchedulerGetTimesliceModeProperties(
    zes_sched_handle_t hScheduler,
    ze_bool_t getDefaults,
    zes_sched_timeslice_properties_t *pConfig) {
    return L0::zesSchedulerGetTimesliceModeProperties(
        hScheduler,
        getDefaults,
        pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesSchedulerSetTimeoutMode(
    zes_sched_handle_t hScheduler,
    zes_sched_timeout_properties_t *pProperties,
    ze_bool_t *pNeedReload) {
    return L0::zesSchedulerSetTimeoutMode(
        hScheduler,
        pProperties,
        pNeedReload);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesSchedulerSetTimesliceMode(
    zes_sched_handle_t hScheduler,
    zes_sched_timeslice_properties_t *pProperties,
    ze_bool_t *pNeedReload) {
    return L0::zesSchedulerSetTimesliceMode(
        hScheduler,
        pProperties,
        pNeedReload);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesSchedulerSetExclusiveMode(
    zes_sched_handle_t hScheduler,
    ze_bool_t *pNeedReload) {
    return L0::zesSchedulerSetExclusiveMode(
        hScheduler,
        pNeedReload);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesSchedulerSetComputeUnitDebugMode(
    zes_sched_handle_t hScheduler,
    ze_bool_t *pNeedReload) {
    return L0::zesSchedulerSetComputeUnitDebugMode(
        hScheduler,
        pNeedReload);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumStandbyDomains(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_standby_handle_t *phStandby) {
    return L0::zesDeviceEnumStandbyDomains(
        hDevice,
        pCount,
        phStandby);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesStandbyGetProperties(
    zes_standby_handle_t hStandby,
    zes_standby_properties_t *pProperties) {
    return L0::zesStandbyGetProperties(
        hStandby,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesStandbyGetMode(
    zes_standby_handle_t hStandby,
    zes_standby_promo_mode_t *pMode) {
    return L0::zesStandbyGetMode(
        hStandby,
        pMode);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesStandbySetMode(
    zes_standby_handle_t hStandby,
    zes_standby_promo_mode_t mode) {
    return L0::zesStandbySetMode(
        hStandby,
        mode);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesDeviceEnumTemperatureSensors(
    zes_device_handle_t hDevice,
    uint32_t *pCount,
    zes_temp_handle_t *phTemperature) {
    return L0::zesDeviceEnumTemperatureSensors(
        hDevice,
        pCount,
        phTemperature);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesTemperatureGetProperties(
    zes_temp_handle_t hTemperature,
    zes_temp_properties_t *pProperties) {
    return L0::zesTemperatureGetProperties(
        hTemperature,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesTemperatureGetConfig(
    zes_temp_handle_t hTemperature,
    zes_temp_config_t *pConfig) {
    return L0::zesTemperatureGetConfig(
        hTemperature,
        pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesTemperatureSetConfig(
    zes_temp_handle_t hTemperature,
    const zes_temp_config_t *pConfig) {
    return L0::zesTemperatureSetConfig(
        hTemperature,
        pConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zesTemperatureGetState(
    zes_temp_handle_t hTemperature,
    double *pTemperature) {
    return L0::zesTemperatureGetState(
        hTemperature,
        pTemperature);
}
}
