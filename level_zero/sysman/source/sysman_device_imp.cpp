/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/sysman_device_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/os_sysman.h"

#include <vector>

namespace L0 {
namespace Sysman {

SysmanDeviceImp::SysmanDeviceImp(NEO::ExecutionEnvironment *executionEnvironment, const uint32_t rootDeviceIndex)
    : executionEnvironment(executionEnvironment), rootDeviceIndex(rootDeviceIndex) {
    this->executionEnvironment->incRefInternal();
    pOsSysman = OsSysman::create(this);
    UNRECOVERABLE_IF(nullptr == pOsSysman);
    pFabricPortHandleContext = new FabricPortHandleContext(pOsSysman);
    pMemoryHandleContext = new MemoryHandleContext(pOsSysman);
    pPowerHandleContext = new PowerHandleContext(pOsSysman);
    pEngineHandleContext = new EngineHandleContext(pOsSysman);
}

SysmanDeviceImp::~SysmanDeviceImp() {
    executionEnvironment->decRefInternal();
    freeResource(pPowerHandleContext);
    freeResource(pEngineHandleContext);
    freeResource(pFabricPortHandleContext);
    freeResource(pMemoryHandleContext);
    freeResource(pOsSysman);
}

ze_result_t SysmanDeviceImp::init() {
    auto result = pOsSysman->init();
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    return result;
}

ze_result_t SysmanDeviceImp::fabricPortGet(uint32_t *pCount, zes_fabric_port_handle_t *phPort) {
    return pFabricPortHandleContext->fabricPortGet(pCount, phPort);
}

ze_result_t SysmanDeviceImp::memoryGet(uint32_t *pCount, zes_mem_handle_t *phMemory) {
    return pMemoryHandleContext->memoryGet(pCount, phMemory);
}

ze_result_t SysmanDeviceImp::powerGetCardDomain(zes_pwr_handle_t *phPower) {
    return pPowerHandleContext->powerGetCardDomain(phPower);
}

ze_result_t SysmanDeviceImp::powerGet(uint32_t *pCount, zes_pwr_handle_t *phPower) {
    return pPowerHandleContext->powerGet(pCount, phPower);
}

ze_result_t SysmanDeviceImp::engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine) {
    return pEngineHandleContext->engineGet(pCount, phEngine);
}

} // namespace Sysman
} // namespace L0
