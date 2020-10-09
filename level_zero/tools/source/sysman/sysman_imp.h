/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/os_sysman.h"
#include "level_zero/tools/source/sysman/sysman.h"
#include <level_zero/zes_api.h>

#include <unordered_map>

namespace L0 {

struct SysmanDeviceImp : SysmanDevice, NEO::NonCopyableOrMovableClass {

    SysmanDeviceImp(ze_device_handle_t hDevice);
    ~SysmanDeviceImp() override;

    SysmanDeviceImp() = delete;
    void init();

    ze_device_handle_t hCoreDevice = nullptr;
    OsSysman *pOsSysman = nullptr;
    Pci *pPci = nullptr;
    GlobalOperations *pGlobalOperations = nullptr;
    Events *pEvents = nullptr;
    PowerHandleContext *pPowerHandleContext = nullptr;
    FrequencyHandleContext *pFrequencyHandleContext = nullptr;
    FabricPortHandleContext *pFabricPortHandleContext = nullptr;
    TemperatureHandleContext *pTempHandleContext = nullptr;
    StandbyHandleContext *pStandbyHandleContext = nullptr;
    EngineHandleContext *pEngineHandleContext = nullptr;
    SchedulerHandleContext *pSchedulerHandleContext = nullptr;
    RasHandleContext *pRasHandleContext = nullptr;
    MemoryHandleContext *pMemoryHandleContext = nullptr;
    FanHandleContext *pFanHandleContext = nullptr;
    FirmwareHandleContext *pFirmwareHandleContext = nullptr;

    ze_result_t powerGet(uint32_t *pCount, zes_pwr_handle_t *phPower) override;
    ze_result_t frequencyGet(uint32_t *pCount, zes_freq_handle_t *phFrequency) override;
    ze_result_t fabricPortGet(uint32_t *pCount, zes_fabric_port_handle_t *phPort) override;
    ze_result_t temperatureGet(uint32_t *pCount, zes_temp_handle_t *phTemperature) override;
    ze_result_t standbyGet(uint32_t *pCount, zes_standby_handle_t *phStandby) override;
    ze_result_t deviceGetProperties(zes_device_properties_t *pProperties) override;
    ze_result_t processesGetState(uint32_t *pCount, zes_process_state_t *pProcesses) override;
    ze_result_t deviceReset(ze_bool_t force) override;
    ze_result_t deviceGetState(zes_device_state_t *pState) override;
    ze_result_t engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine) override;
    ze_result_t pciGetProperties(zes_pci_properties_t *pProperties) override;
    ze_result_t pciGetState(zes_pci_state_t *pState) override;
    ze_result_t pciGetBars(uint32_t *pCount, zes_pci_bar_properties_t *pProperties) override;
    ze_result_t pciGetStats(zes_pci_stats_t *pStats) override;
    ze_result_t schedulerGet(uint32_t *pCount, zes_sched_handle_t *phScheduler) override;
    ze_result_t rasGet(uint32_t *pCount, zes_ras_handle_t *phRas) override;
    ze_result_t memoryGet(uint32_t *pCount, zes_mem_handle_t *phMemory) override;
    ze_result_t fanGet(uint32_t *pCount, zes_fan_handle_t *phFan) override;
    ze_result_t firmwareGet(uint32_t *pCount, zes_firmware_handle_t *phFirmware) override;
    ze_result_t deviceEventRegister(zes_event_type_flags_t events) override;
    bool deviceEventListen(zes_event_type_flags_t &pEvent) override;

  private:
    template <typename T>
    void inline freeResource(T *&resource) {
        if (resource) {
            delete resource;
            resource = nullptr;
        }
    }
};

} // namespace L0
