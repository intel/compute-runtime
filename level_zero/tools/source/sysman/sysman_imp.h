/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/os_sysman.h"
#include "level_zero/tools/source/sysman/sysman.h"
#include <level_zero/zet_api.h>
#include <level_zero/zet_sysman.h>

#include <unordered_map>

namespace L0 {

struct SysmanImp : Sysman {

    SysmanImp(ze_device_handle_t hDevice);
    ~SysmanImp() override;

    SysmanImp() = delete;
    SysmanImp(const SysmanImp &obj) = delete;
    SysmanImp &operator=(const SysmanImp &obj) = delete;

    void init();

    ze_device_handle_t hCoreDevice;

    OsSysman *pOsSysman;
    Pci *pPci;
    sysmanDevice *pSysmanDevice;
    FrequencyHandleContext *pFrequencyHandleContext;
    StandbyHandleContext *pStandbyHandleContext;

    ze_result_t deviceGetProperties(zet_sysman_properties_t *pProperties) override;
    ze_result_t schedulerGetCurrentMode(zet_sched_mode_t *pMode) override;
    ze_result_t schedulerGetTimeoutModeProperties(ze_bool_t getDefaults, zet_sched_timeout_properties_t *pConfig) override;
    ze_result_t schedulerGetTimesliceModeProperties(ze_bool_t getDefaults, zet_sched_timeslice_properties_t *pConfig) override;
    ze_result_t schedulerSetTimeoutMode(zet_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReboot) override;
    ze_result_t schedulerSetTimesliceMode(zet_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReboot) override;
    ze_result_t schedulerSetExclusiveMode(ze_bool_t *pNeedReboot) override;
    ze_result_t schedulerSetComputeUnitDebugMode(ze_bool_t *pNeedReboot) override;
    ze_result_t processesGetState(uint32_t *pCount, zet_process_state_t *pProcesses) override;
    ze_result_t deviceReset() override;
    ze_result_t deviceGetRepairStatus(zet_repair_status_t *pRepairStatus) override;
    ze_result_t pciGetProperties(zet_pci_properties_t *pProperties) override;
    ze_result_t pciGetState(zet_pci_state_t *pState) override;
    ze_result_t pciGetBars(uint32_t *pCount, zet_pci_bar_properties_t *pProperties) override;
    ze_result_t pciGetStats(zet_pci_stats_t *pStats) override;
    ze_result_t powerGet(uint32_t *pCount, zet_sysman_pwr_handle_t *phPower) override;
    ze_result_t frequencyGet(uint32_t *pCount, zet_sysman_freq_handle_t *phFrequency) override;
    ze_result_t engineGet(uint32_t *pCount, zet_sysman_engine_handle_t *phEngine) override;
    ze_result_t standbyGet(uint32_t *pCount, zet_sysman_standby_handle_t *phStandby) override;
    ze_result_t firmwareGet(uint32_t *pCount, zet_sysman_firmware_handle_t *phFirmware) override;
    ze_result_t memoryGet(uint32_t *pCount, zet_sysman_mem_handle_t *phMemory) override;
    ze_result_t fabricPortGet(uint32_t *pCount, zet_sysman_fabric_port_handle_t *phPort) override;
    ze_result_t temperatureGet(uint32_t *pCount, zet_sysman_temp_handle_t *phTemperature) override;
    ze_result_t psuGet(uint32_t *pCount, zet_sysman_psu_handle_t *phPsu) override;
    ze_result_t fanGet(uint32_t *pCount, zet_sysman_fan_handle_t *phFan) override;
    ze_result_t ledGet(uint32_t *pCount, zet_sysman_led_handle_t *phLed) override;
    ze_result_t rasGet(uint32_t *pCount, zet_sysman_ras_handle_t *phRas) override;
    ze_result_t eventGet(zet_sysman_event_handle_t *phEvent) override;
    ze_result_t diagnosticsGet(uint32_t *pCount, zet_sysman_diag_handle_t *phDiagnostics) override;
};

} // namespace L0
