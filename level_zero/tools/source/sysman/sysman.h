/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/engine/engine.h"
#include "level_zero/tools/source/sysman/events/events.h"
#include "level_zero/tools/source/sysman/fabric_port/fabric_port.h"
#include "level_zero/tools/source/sysman/fan/fan.h"
#include "level_zero/tools/source/sysman/firmware/firmware.h"
#include "level_zero/tools/source/sysman/frequency/frequency.h"
#include "level_zero/tools/source/sysman/global_operations/global_operations.h"
#include "level_zero/tools/source/sysman/memory/memory.h"
#include "level_zero/tools/source/sysman/pci/pci.h"
#include "level_zero/tools/source/sysman/power/power.h"
#include "level_zero/tools/source/sysman/ras/ras.h"
#include "level_zero/tools/source/sysman/scheduler/scheduler.h"
#include "level_zero/tools/source/sysman/standby/standby.h"
#include "level_zero/tools/source/sysman/temperature/temperature.h"
#include <level_zero/zes_api.h>

struct _zet_sysman_handle_t {};

namespace L0 {
struct Device;
struct SysmanDevice : _ze_device_handle_t {

    static SysmanDevice *fromHandle(zes_device_handle_t handle) { return Device::fromHandle(handle)->getSysmanHandle(); }

    virtual ze_result_t powerGet(uint32_t *pCount, zes_pwr_handle_t *phPower) = 0;
    virtual ze_result_t frequencyGet(uint32_t *pCount, zes_freq_handle_t *phFrequency) = 0;
    virtual ze_result_t fabricPortGet(uint32_t *pCount, zes_fabric_port_handle_t *phPort) = 0;
    virtual ze_result_t temperatureGet(uint32_t *pCount, zes_temp_handle_t *phTemperature) = 0;
    virtual ze_result_t standbyGet(uint32_t *pCount, zes_standby_handle_t *phStandby) = 0;
    virtual ze_result_t deviceGetProperties(zes_device_properties_t *pProperties) = 0;
    virtual ze_result_t processesGetState(uint32_t *pCount, zes_process_state_t *pProcesses) = 0;
    virtual ze_result_t deviceReset(ze_bool_t force) = 0;
    virtual ze_result_t deviceGetState(zes_device_state_t *pState) = 0;
    virtual ze_result_t engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine) = 0;
    virtual ze_result_t pciGetProperties(zes_pci_properties_t *pProperties) = 0;
    virtual ze_result_t pciGetState(zes_pci_state_t *pState) = 0;
    virtual ze_result_t pciGetBars(uint32_t *pCount, zes_pci_bar_properties_t *pProperties) = 0;
    virtual ze_result_t pciGetStats(zes_pci_stats_t *pStats) = 0;
    virtual ze_result_t schedulerGet(uint32_t *pCount, zes_sched_handle_t *phScheduler) = 0;
    virtual ze_result_t rasGet(uint32_t *pCount, zes_ras_handle_t *phRas) = 0;
    virtual ze_result_t memoryGet(uint32_t *pCount, zes_mem_handle_t *phMemory) = 0;
    virtual ze_result_t fanGet(uint32_t *pCount, zes_fan_handle_t *phFan) = 0;
    virtual ze_result_t firmwareGet(uint32_t *pCount, zes_firmware_handle_t *phFirmware) = 0;
    virtual ze_result_t deviceEventRegister(zes_event_type_flags_t events) = 0;
    virtual bool deviceEventListen(zes_event_type_flags_t &pEvent) = 0;
    virtual ~SysmanDevice() = default;
};

class SysmanDeviceHandleContext {
  public:
    SysmanDeviceHandleContext() = delete;
    static SysmanDevice *init(ze_device_handle_t device);
};

} // namespace L0
