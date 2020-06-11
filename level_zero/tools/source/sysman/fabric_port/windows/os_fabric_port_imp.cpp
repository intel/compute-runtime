/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fabric_port/windows/os_fabric_port_imp.h"

#include <cstring>

namespace L0 {

ze_result_t WddmFabricPortImp::getLinkType(ze_bool_t verbose, zet_fabric_link_type_t *pLinkType) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFabricPortImp::getConfig(zet_fabric_port_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFabricPortImp::setConfig(const zet_fabric_port_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFabricPortImp::getState(zet_fabric_port_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFabricPortImp::getThroughput(zet_fabric_port_throughput_t *pThroughput) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void WddmFabricPortImp::getModel(int8_t *model) {
    ::memset(model, '\0', ZET_MAX_FABRIC_PORT_MODEL_SIZE);
}

void WddmFabricPortImp::getPortUuid(zet_fabric_port_uuid_t &portUuid) {
    ::memset(&portUuid, '\0', sizeof(portUuid));
}

void WddmFabricPortImp::getMaxRxSpeed(zet_fabric_port_speed_t &maxRxSpeed) {
    ::memset(&maxRxSpeed, '\0', sizeof(maxRxSpeed));
}

void WddmFabricPortImp::getMaxTxSpeed(zet_fabric_port_speed_t &maxTxSpeed) {
    ::memset(&maxTxSpeed, '\0', sizeof(maxTxSpeed));
}

WddmFabricPortImp::WddmFabricPortImp(OsSysman *pOsSysman, uint32_t portNum) {
}

WddmFabricPortImp::~WddmFabricPortImp() {
}

OsFabricPort *OsFabricPort::create(OsSysman *pOsSysman, uint32_t portNum) {
    WddmFabricPortImp *pWddmFabricPortImp = new WddmFabricPortImp(pOsSysman, portNum);
    return static_cast<OsFabricPort *>(pWddmFabricPortImp);
}

uint32_t OsFabricPort::numPorts(OsSysman *pOsSysman) {
    return 0U;
}

} // namespace L0
