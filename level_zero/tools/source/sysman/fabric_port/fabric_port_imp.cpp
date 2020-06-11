/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fabric_port/fabric_port_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include <chrono>

namespace L0 {

void fabricPortGetTimestamp(uint64_t &timestamp) {
    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();
}

ze_result_t FabricPortImp::fabricPortGetProperties(zet_fabric_port_properties_t *pProperties) {
    *pProperties = fabricPortProperties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricPortImp::fabricPortGetLinkType(ze_bool_t verbose, zet_fabric_link_type_t *pLinkType) {
    return pOsFabricPort->getLinkType(verbose, pLinkType);
}

ze_result_t FabricPortImp::fabricPortGetConfig(zet_fabric_port_config_t *pConfig) {
    return pOsFabricPort->getConfig(pConfig);
}

ze_result_t FabricPortImp::fabricPortSetConfig(const zet_fabric_port_config_t *pConfig) {
    return pOsFabricPort->setConfig(pConfig);
}

ze_result_t FabricPortImp::fabricPortGetState(zet_fabric_port_state_t *pState) {
    return pOsFabricPort->getState(pState);
}

ze_result_t FabricPortImp::fabricPortGetThroughput(zet_fabric_port_throughput_t *pThroughput) {
    fabricPortGetTimestamp(pThroughput->timestamp);
    ze_result_t result = pOsFabricPort->getThroughput(pThroughput);
    return result;
}

void FabricPortImp::init() {
    fabricPortProperties.onSubdevice = false;
    fabricPortProperties.subdeviceId = 0L;
    pOsFabricPort->getModel(fabricPortProperties.model);
    pOsFabricPort->getPortUuid(fabricPortProperties.portUuid);
    pOsFabricPort->getMaxRxSpeed(fabricPortProperties.maxRxSpeed);
    pOsFabricPort->getMaxTxSpeed(fabricPortProperties.maxTxSpeed);
}

FabricPortImp::FabricPortImp(OsSysman *pOsSysman, uint32_t portNum) {
    pOsFabricPort = OsFabricPort::create(pOsSysman, portNum);
    UNRECOVERABLE_IF(nullptr == pOsFabricPort);

    init();
}

FabricPortImp::~FabricPortImp() {
    delete pOsFabricPort;
    pOsFabricPort = nullptr;
}

uint32_t FabricPortImp::numPorts(OsSysman *pOsSysman) {
    return OsFabricPort::numPorts(pOsSysman);
}

} // namespace L0
