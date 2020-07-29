/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/sysman_imp.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/tools/source/sysman/global_operations/global_operations_imp.h"
#include "level_zero/tools/source/sysman/pci/pci_imp.h"
#include "level_zero/tools/source/sysman/scheduler/scheduler_imp.h"
#include "level_zero/tools/source/sysman/sysman.h"

#include <vector>

namespace L0 {

SysmanDeviceImp::SysmanDeviceImp(ze_device_handle_t hDevice) {
    hCoreDevice = hDevice;
    pOsSysman = OsSysman::create(this);
    UNRECOVERABLE_IF(nullptr == pOsSysman);
    pPowerHandleContext = new PowerHandleContext(pOsSysman);
    pFrequencyHandleContext = new FrequencyHandleContext(pOsSysman);
    pFabricPortHandleContext = new FabricPortHandleContext(pOsSysman);
    pTempHandleContext = new TemperatureHandleContext(pOsSysman);
    pStandbyHandleContext = new StandbyHandleContext(pOsSysman);
    pEngineHandleContext = new EngineHandleContext(pOsSysman);
}

SysmanDeviceImp::~SysmanDeviceImp() {
    freeResource(pEngineHandleContext);
    freeResource(pStandbyHandleContext);
    freeResource(pTempHandleContext);
    freeResource(pFabricPortHandleContext);
    freeResource(pFrequencyHandleContext);
    freeResource(pPowerHandleContext);
    freeResource(pOsSysman);
}

void SysmanDeviceImp::init() {
    pOsSysman->init();
    if (pPowerHandleContext) {
        pPowerHandleContext->init();
    }
    if (pFrequencyHandleContext) {
        pFrequencyHandleContext->init();
    }
    if (pFabricPortHandleContext) {
        pFabricPortHandleContext->init();
    }
    if (pTempHandleContext) {
        pTempHandleContext->init();
    }
    if (pStandbyHandleContext) {
        pStandbyHandleContext->init();
    }
    if (pEngineHandleContext) {
        pEngineHandleContext->init();
    }
}

ze_result_t SysmanDeviceImp::frequencyGet(uint32_t *pCount, zes_freq_handle_t *phFrequency) {
    return pFrequencyHandleContext->frequencyGet(pCount, phFrequency);
}

ze_result_t SysmanDeviceImp::powerGet(uint32_t *pCount, zes_pwr_handle_t *phPower) {
    return pPowerHandleContext->powerGet(pCount, phPower);
}

ze_result_t SysmanDeviceImp::engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine) {
    return pEngineHandleContext->engineGet(pCount, phEngine);
}

ze_result_t SysmanDeviceImp::standbyGet(uint32_t *pCount, zes_standby_handle_t *phStandby) {
    return pStandbyHandleContext->standbyGet(pCount, phStandby);
}

ze_result_t SysmanDeviceImp::fabricPortGet(uint32_t *pCount, zes_fabric_port_handle_t *phPort) {
    return pFabricPortHandleContext->fabricPortGet(pCount, phPort);
}

ze_result_t SysmanDeviceImp::temperatureGet(uint32_t *pCount, zes_temp_handle_t *phTemperature) {
    return pTempHandleContext->temperatureGet(pCount, phTemperature);
}

} // namespace L0
