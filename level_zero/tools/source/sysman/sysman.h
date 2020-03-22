/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/frequency/frequency.h"
#include "level_zero/tools/source/sysman/pci/pci.h"
#include "level_zero/tools/source/sysman/standby/standby.h"
#include "level_zero/tools/source/sysman/sysman_device/sysman_device.h"
#include <level_zero/zet_api.h>
#include <level_zero/zet_sysman.h>

#include <unordered_map>

struct _zet_sysman_handle_t {};

namespace L0 {

struct Sysman : _zet_sysman_handle_t {

    static Sysman *fromHandle(zet_sysman_handle_t handle) { return static_cast<Sysman *>(handle); }
    inline zet_sysman_handle_t toHandle() { return this; }

    virtual ze_result_t deviceGetProperties(zet_sysman_properties_t *pProperties) = 0;
    virtual ze_result_t schedulerGetCurrentMode(zet_sched_mode_t *pMode) = 0;
    virtual ze_result_t schedulerGetTimeoutModeProperties(ze_bool_t getDefaults, zet_sched_timeout_properties_t *pConfig) = 0;
    virtual ze_result_t schedulerGetTimesliceModeProperties(ze_bool_t getDefaults, zet_sched_timeslice_properties_t *pConfig) = 0;
    virtual ze_result_t schedulerSetTimeoutMode(zet_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReboot) = 0;
    virtual ze_result_t schedulerSetTimesliceMode(zet_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReboot) = 0;
    virtual ze_result_t schedulerSetExclusiveMode(ze_bool_t *pNeedReboot) = 0;
    virtual ze_result_t schedulerSetComputeUnitDebugMode(ze_bool_t *pNeedReboot) = 0;
    virtual ze_result_t processesGetState(uint32_t *pCount, zet_process_state_t *pProcesses) = 0;
    virtual ze_result_t deviceReset() = 0;
    virtual ze_result_t deviceGetRepairStatus(zet_repair_status_t *pRepairStatus) = 0;
    virtual ze_result_t pciGetProperties(zet_pci_properties_t *pProperties) = 0;
    virtual ze_result_t pciGetState(zet_pci_state_t *pState) = 0;
    virtual ze_result_t pciGetBars(uint32_t *pCount, zet_pci_bar_properties_t *pProperties) = 0;
    virtual ze_result_t pciGetStats(zet_pci_stats_t *pStats) = 0;
    virtual ze_result_t powerGet(uint32_t *pCount, zet_sysman_pwr_handle_t *phPower) = 0;
    virtual ze_result_t frequencyGet(uint32_t *pCount, zet_sysman_freq_handle_t *phFrequency) = 0;
    virtual ze_result_t engineGet(uint32_t *pCount, zet_sysman_engine_handle_t *phEngine) = 0;
    virtual ze_result_t standbyGet(uint32_t *pCount, zet_sysman_standby_handle_t *phStandby) = 0;
    virtual ze_result_t firmwareGet(uint32_t *pCount, zet_sysman_firmware_handle_t *phFirmware) = 0;
    virtual ze_result_t memoryGet(uint32_t *pCount, zet_sysman_mem_handle_t *phMemory) = 0;
    virtual ze_result_t fabricPortGet(uint32_t *pCount, zet_sysman_fabric_port_handle_t *phPort) = 0;
    virtual ze_result_t temperatureGet(uint32_t *pCount, zet_sysman_temp_handle_t *phTemperature) = 0;
    virtual ze_result_t psuGet(uint32_t *pCount, zet_sysman_psu_handle_t *phPsu) = 0;
    virtual ze_result_t fanGet(uint32_t *pCount, zet_sysman_fan_handle_t *phFan) = 0;
    virtual ze_result_t ledGet(uint32_t *pCount, zet_sysman_led_handle_t *phLed) = 0;
    virtual ze_result_t rasGet(uint32_t *pCount, zet_sysman_ras_handle_t *phRas) = 0;
    virtual ze_result_t eventGet(zet_sysman_event_handle_t *phEvent) = 0;
    virtual ze_result_t diagnosticsGet(uint32_t *pCount, zet_sysman_diag_handle_t *phDiagnostics) = 0;

    virtual ~Sysman() = default;
};

class SysmanHandleContext {
  public:
    SysmanHandleContext();
    static void init(ze_init_flag_t flag);
    static ze_result_t sysmanGet(zet_device_handle_t hDevice, zet_sysman_handle_t *phSysman);

  private:
    std::unordered_map<zet_device_handle_t, zet_sysman_handle_t> handle_map;
};

} // namespace L0
