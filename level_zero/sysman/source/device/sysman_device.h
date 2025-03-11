/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/sysman/source/api/diagnostics/sysman_diagnostics.h"
#include "level_zero/sysman/source/api/ecc/sysman_ecc.h"
#include "level_zero/sysman/source/api/engine/sysman_engine.h"
#include "level_zero/sysman/source/api/events/sysman_events.h"
#include "level_zero/sysman/source/api/fabric_port/sysman_fabric_port.h"
#include "level_zero/sysman/source/api/fan/sysman_fan.h"
#include "level_zero/sysman/source/api/firmware/sysman_firmware.h"
#include "level_zero/sysman/source/api/frequency/sysman_frequency.h"
#include "level_zero/sysman/source/api/global_operations/sysman_global_operations.h"
#include "level_zero/sysman/source/api/memory/sysman_memory.h"
#include "level_zero/sysman/source/api/pci/sysman_pci.h"
#include "level_zero/sysman/source/api/performance/sysman_performance.h"
#include "level_zero/sysman/source/api/power/sysman_power.h"
#include "level_zero/sysman/source/api/ras/sysman_ras.h"
#include "level_zero/sysman/source/api/scheduler/sysman_scheduler.h"
#include "level_zero/sysman/source/api/standby/sysman_standby.h"
#include "level_zero/sysman/source/api/temperature/sysman_temperature.h"
#include "level_zero/sysman/source/api/vf_management/sysman_vf_management.h"
#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

namespace L0 {
namespace Sysman {

struct SysmanDevice : _ze_device_handle_t {
    static SysmanDevice *fromHandle(zes_device_handle_t handle);
    inline zes_device_handle_t toHandle() { return this; }
    virtual ~SysmanDevice() = default;
    static SysmanDevice *create(NEO::ExecutionEnvironment &executionEnvironment, const uint32_t rootDeviceIndex);
    virtual const NEO::HardwareInfo &getHardwareInfo() const = 0;

    static ze_result_t powerGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_pwr_handle_t *phPower);
    virtual ze_result_t powerGet(uint32_t *pCount, zes_pwr_handle_t *phPower) = 0;

    static ze_result_t powerGetCardDomain(zes_device_handle_t hDevice, zes_pwr_handle_t *phPower);
    virtual ze_result_t powerGetCardDomain(zes_pwr_handle_t *phPower) = 0;

    static ze_result_t fabricPortGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_fabric_port_handle_t *phPort);
    virtual ze_result_t fabricPortGet(uint32_t *pCount, zes_fabric_port_handle_t *phPort) = 0;

    static ze_result_t memoryGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_mem_handle_t *phMemory);
    virtual ze_result_t memoryGet(uint32_t *pCount, zes_mem_handle_t *phMemory) = 0;

    static ze_result_t engineGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_engine_handle_t *phEngine);
    virtual ze_result_t engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine) = 0;

    static ze_result_t frequencyGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_freq_handle_t *phFrequency);
    virtual ze_result_t frequencyGet(uint32_t *pCount, zes_freq_handle_t *phFrequency) = 0;

    static ze_result_t schedulerGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_sched_handle_t *phScheduler);
    virtual ze_result_t schedulerGet(uint32_t *pCount, zes_sched_handle_t *phScheduler) = 0;

    static ze_result_t firmwareGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_firmware_handle_t *phFirmware);
    virtual ze_result_t firmwareGet(uint32_t *pCount, zes_firmware_handle_t *phFirmware) = 0;

    static ze_result_t diagnosticsGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_diag_handle_t *phDiagnostics);
    virtual ze_result_t diagnosticsGet(uint32_t *pCount, zes_diag_handle_t *phDiagnostics) = 0;

    static ze_result_t rasGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_ras_handle_t *phRas);
    virtual ze_result_t rasGet(uint32_t *pCount, zes_ras_handle_t *phRas) = 0;

    static ze_result_t deviceReset(zes_device_handle_t hDevice, ze_bool_t force);
    virtual ze_result_t deviceReset(ze_bool_t force) = 0;

    static ze_result_t deviceGetProperties(zes_device_handle_t hDevice, zes_device_properties_t *pProperties);
    virtual ze_result_t deviceGetProperties(zes_device_properties_t *pProperties) = 0;

    static ze_result_t deviceGetSubDeviceProperties(zes_device_handle_t hDevice, uint32_t *pCount, zes_subdevice_exp_properties_t *pSubdeviceProps);
    virtual ze_result_t deviceGetSubDeviceProperties(uint32_t *pCount, zes_subdevice_exp_properties_t *pSubdeviceProps) = 0;
    virtual ze_bool_t getDeviceInfoByUuid(zes_uuid_t uuid, ze_bool_t *onSubdevice, uint32_t *subdeviceId) = 0;

    static ze_result_t deviceGetState(zes_device_handle_t hDevice, zes_device_state_t *pState);
    virtual ze_result_t deviceGetState(zes_device_state_t *pState) = 0;

    static ze_result_t processesGetState(zes_device_handle_t hDevice, uint32_t *pCount, zes_process_state_t *pProcesses);
    virtual ze_result_t processesGetState(uint32_t *pCount, zes_process_state_t *pProcesses) = 0;

    static ze_result_t standbyGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_standby_handle_t *phStandby);
    virtual ze_result_t standbyGet(uint32_t *pCount, zes_standby_handle_t *phStandby) = 0;

    static ze_result_t deviceEccAvailable(zes_device_handle_t hDevice, ze_bool_t *pAvailable);
    virtual ze_result_t deviceEccAvailable(ze_bool_t *pAvailable) = 0;

    static ze_result_t deviceEccConfigurable(zes_device_handle_t hDevice, ze_bool_t *pConfigurable);
    virtual ze_result_t deviceEccConfigurable(ze_bool_t *pConfigurable) = 0;

    static ze_result_t deviceGetEccState(zes_device_handle_t hDevice, zes_device_ecc_properties_t *pState);
    virtual ze_result_t deviceGetEccState(zes_device_ecc_properties_t *pState) = 0;

    static ze_result_t deviceSetEccState(zes_device_handle_t hDevice, const zes_device_ecc_desc_t *newState, zes_device_ecc_properties_t *pState);
    virtual ze_result_t deviceSetEccState(const zes_device_ecc_desc_t *newState, zes_device_ecc_properties_t *pState) = 0;

    static ze_result_t temperatureGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_temp_handle_t *phTemperature);
    virtual ze_result_t temperatureGet(uint32_t *pCount, zes_temp_handle_t *phTemperature) = 0;

    static ze_result_t performanceGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_perf_handle_t *phPerformance);
    virtual ze_result_t performanceGet(uint32_t *pCount, zes_perf_handle_t *phPerformance) = 0;

    static ze_result_t pciGetProperties(zes_device_handle_t hDevice, zes_pci_properties_t *pProperties);
    virtual ze_result_t pciGetProperties(zes_pci_properties_t *pProperties) = 0;

    static ze_result_t pciGetState(zes_device_handle_t hDevice, zes_pci_state_t *pState);
    virtual ze_result_t pciGetState(zes_pci_state_t *pState) = 0;

    static ze_result_t pciGetBars(zes_device_handle_t hDevice, uint32_t *pCount, zes_pci_bar_properties_t *pProperties);
    virtual ze_result_t pciGetBars(uint32_t *pCount, zes_pci_bar_properties_t *pProperties) = 0;

    static ze_result_t pciGetStats(zes_device_handle_t hDevice, zes_pci_stats_t *pStats);
    virtual ze_result_t pciGetStats(zes_pci_stats_t *pStats) = 0;

    static ze_result_t fanGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_fan_handle_t *phFan);
    virtual ze_result_t fanGet(uint32_t *pCount, zes_fan_handle_t *phFan) = 0;

    static ze_result_t deviceEventRegister(zes_device_handle_t hDevice, zes_event_type_flags_t events);
    virtual ze_result_t deviceEventRegister(zes_event_type_flags_t events) = 0;

    virtual bool deviceEventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) = 0;
    static uint64_t getSysmanTimestamp();

    virtual ze_result_t deviceResetExt(zes_reset_properties_t *pProperties) = 0;
    static ze_result_t deviceResetExt(zes_device_handle_t hDevice, zes_reset_properties_t *pProperties);

    virtual ze_result_t fabricPortGetMultiPortThroughput(uint32_t numPorts, zes_fabric_port_handle_t *phPort, zes_fabric_port_throughput_t **pThroughput) = 0;
    static ze_result_t fabricPortGetMultiPortThroughput(zes_device_handle_t hDevice, uint32_t numPorts, zes_fabric_port_handle_t *phPort, zes_fabric_port_throughput_t **pThroughput);

    virtual ze_result_t deviceEnumEnabledVF(uint32_t *pCount, zes_vf_handle_t *phVFhandle) = 0;
    static ze_result_t deviceEnumEnabledVF(zes_device_handle_t hDevice, uint32_t *pCount, zes_vf_handle_t *phVFhandle);

    virtual OsSysman *deviceGetOsInterface() = 0;
    virtual void getDeviceUuids(std::vector<std::string> &deviceUuids) = 0;
};

} // namespace Sysman
} // namespace L0
