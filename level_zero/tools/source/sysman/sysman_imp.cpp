/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/sysman_imp.h"

#include "level_zero/core/source/driver.h"
#include "level_zero/core/source/driver_handle_imp.h"
#include "level_zero/tools/source/sysman/pci/pci_imp.h"
#include "level_zero/tools/source/sysman/sysman.h"
#include "level_zero/tools/source/sysman/sysman_device/sysman_device_imp.h"

#include <vector>

namespace L0 {

SysmanImp::SysmanImp(ze_device_handle_t hDevice) {
    hCoreDevice = hDevice;
    pOsSysman = OsSysman::create(this);
    pPci = new PciImp(pOsSysman);
    pSysmanDevice = new SysmanDeviceImp(pOsSysman, hCoreDevice);
    pFrequencyHandleContext = new FrequencyHandleContext(pOsSysman);
    pStandbyHandleContext = new StandbyHandleContext(pOsSysman);
}

SysmanImp::~SysmanImp() {
    delete pStandbyHandleContext;
    delete pFrequencyHandleContext;
    delete pPci;
    delete pSysmanDevice;
    delete pOsSysman;
}

void SysmanImp::init() {
    pOsSysman->init();
    pFrequencyHandleContext->init();
    pStandbyHandleContext->init();
    pPci->init();
    pSysmanDevice->init();
}

ze_result_t SysmanImp::deviceGetProperties(zet_sysman_properties_t *pProperties) {
    return pSysmanDevice->deviceGetProperties(pProperties);
}

ze_result_t SysmanImp::schedulerGetCurrentMode(zet_sched_mode_t *pMode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::schedulerGetTimeoutModeProperties(ze_bool_t getDefaults, zet_sched_timeout_properties_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::schedulerGetTimesliceModeProperties(ze_bool_t getDefaults, zet_sched_timeslice_properties_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::schedulerSetTimeoutMode(zet_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReboot) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::schedulerSetTimesliceMode(zet_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReboot) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::schedulerSetExclusiveMode(ze_bool_t *pNeedReboot) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::schedulerSetComputeUnitDebugMode(ze_bool_t *pNeedReboot) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::processesGetState(uint32_t *pCount, zet_process_state_t *pProcesses) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::deviceReset() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::deviceGetRepairStatus(zet_repair_status_t *pRepairStatus) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::pciGetProperties(zet_pci_properties_t *pProperties) {
    return pPci->pciStaticProperties(pProperties);
}

ze_result_t SysmanImp::pciGetState(zet_pci_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::pciGetBars(uint32_t *pCount, zet_pci_bar_properties_t *pProperties) {
    return pPci->pciGetInitializedBars(pCount, pProperties);
}

ze_result_t SysmanImp::pciGetStats(zet_pci_stats_t *pStats) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::powerGet(uint32_t *pCount, zet_sysman_pwr_handle_t *phPower) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::frequencyGet(uint32_t *pCount, zet_sysman_freq_handle_t *phFrequency) {
    return pFrequencyHandleContext->frequencyGet(pCount, phFrequency);
}

ze_result_t SysmanImp::engineGet(uint32_t *pCount, zet_sysman_engine_handle_t *phEngine) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::standbyGet(uint32_t *pCount, zet_sysman_standby_handle_t *phStandby) {
    return pStandbyHandleContext->standbyGet(pCount, phStandby);
}

ze_result_t SysmanImp::firmwareGet(uint32_t *pCount, zet_sysman_firmware_handle_t *phFirmware) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::memoryGet(uint32_t *pCount, zet_sysman_mem_handle_t *phMemory) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::fabricPortGet(uint32_t *pCount, zet_sysman_fabric_port_handle_t *phPort) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::temperatureGet(uint32_t *pCount, zet_sysman_temp_handle_t *phTemperature) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::psuGet(uint32_t *pCount, zet_sysman_psu_handle_t *phPsu) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::fanGet(uint32_t *pCount, zet_sysman_fan_handle_t *phFan) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::ledGet(uint32_t *pCount, zet_sysman_led_handle_t *phLed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::rasGet(uint32_t *pCount, zet_sysman_ras_handle_t *phRas) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::eventGet(zet_sysman_event_handle_t *phEvent) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::diagnosticsGet(uint32_t *pCount, zet_sysman_diag_handle_t *phDiagnostics) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
