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
    PowerHandleContext *pPowerHandleContext = nullptr;
    FrequencyHandleContext *pFrequencyHandleContext = nullptr;
    FabricPortHandleContext *pFabricPortHandleContext = nullptr;
    TemperatureHandleContext *pTempHandleContext = nullptr;
    StandbyHandleContext *pStandbyHandleContext = nullptr;
    EngineHandleContext *pEngineHandleContext = nullptr;

    ze_result_t powerGet(uint32_t *pCount, zes_pwr_handle_t *phPower) override;
    ze_result_t frequencyGet(uint32_t *pCount, zes_freq_handle_t *phFrequency) override;
    ze_result_t fabricPortGet(uint32_t *pCount, zes_fabric_port_handle_t *phPort) override;
    ze_result_t temperatureGet(uint32_t *pCount, zes_temp_handle_t *phTemperature) override;
    ze_result_t standbyGet(uint32_t *pCount, zes_standby_handle_t *phStandby) override;
    ze_result_t engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine) override;

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
