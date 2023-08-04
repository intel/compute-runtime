/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fabric_port/fabric_port_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include <chrono>

namespace L0 {

uint32_t FabricDeviceImp::getNumPorts() {
    UNRECOVERABLE_IF(nullptr == pOsFabricDevice);
    return pOsFabricDevice->getNumPorts();
}

FabricDeviceImp::FabricDeviceImp(OsSysman *pOsSysman) {
    pOsFabricDevice = OsFabricDevice::create(pOsSysman);
    UNRECOVERABLE_IF(nullptr == pOsFabricDevice);
}

FabricDeviceImp::~FabricDeviceImp() {
    delete pOsFabricDevice;
    pOsFabricDevice = nullptr;
}

void fabricPortGetTimestamp(uint64_t &timestamp) {
    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();
}

ze_result_t FabricPortImp::fabricPortGetErrorCounters(zes_fabric_port_error_counters_t *pErrors) {
    return pOsFabricPort->getErrorCounters(pErrors);
}

ze_result_t FabricPortImp::fabricPortGetProperties(zes_fabric_port_properties_t *pProperties) {
    return pOsFabricPort->getProperties(pProperties);
}

ze_result_t FabricPortImp::fabricPortGetLinkType(zes_fabric_link_type_t *pLinkType) {
    return pOsFabricPort->getLinkType(pLinkType);
}

ze_result_t FabricPortImp::fabricPortGetConfig(zes_fabric_port_config_t *pConfig) {
    return pOsFabricPort->getConfig(pConfig);
}

ze_result_t FabricPortImp::fabricPortSetConfig(const zes_fabric_port_config_t *pConfig) {
    return pOsFabricPort->setConfig(pConfig);
}

ze_result_t FabricPortImp::fabricPortGetState(zes_fabric_port_state_t *pState) {
    return pOsFabricPort->getState(pState);
}

ze_result_t FabricPortImp::fabricPortGetThroughput(zes_fabric_port_throughput_t *pThroughput) {
    fabricPortGetTimestamp(pThroughput->timestamp);
    return pOsFabricPort->getThroughput(pThroughput);
}

void FabricPortImp::init() {
}

FabricPortImp::FabricPortImp(FabricDevice *pFabricDevice, uint32_t portNum) {
    pOsFabricPort = OsFabricPort::create(pFabricDevice->getOsFabricDevice(), portNum);
    UNRECOVERABLE_IF(nullptr == pOsFabricPort);

    init();
}

FabricPortImp::~FabricPortImp() = default;

} // namespace L0
