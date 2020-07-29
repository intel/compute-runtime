/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fabric_port/windows/os_fabric_port_imp.h"

#include <cstring>

namespace L0 {

uint32_t WddmFabricDeviceImp::getNumPorts() {
    return numPorts;
}

WddmFabricDeviceImp::WddmFabricDeviceImp(OsSysman *pOsSysman) {
}

WddmFabricDeviceImp::~WddmFabricDeviceImp() {
}

ze_result_t WddmFabricPortImp::getLinkType(zes_fabric_link_type_t *pLinkType) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFabricPortImp::getConfig(zes_fabric_port_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFabricPortImp::setConfig(const zes_fabric_port_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFabricPortImp::getState(zes_fabric_port_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFabricPortImp::getThroughput(zes_fabric_port_throughput_t *pThroughput) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void WddmFabricPortImp::getModel(char *model) {
    ::memset(model, '\0', ZES_MAX_FABRIC_PORT_MODEL_SIZE);
}

void WddmFabricPortImp::getPortId(zes_fabric_port_id_t &portId) {
    ::memset(&portId, '\0', sizeof(portId));
}

void WddmFabricPortImp::getMaxRxSpeed(zes_fabric_port_speed_t &maxRxSpeed) {
    ::memset(&maxRxSpeed, '\0', sizeof(maxRxSpeed));
}

void WddmFabricPortImp::getMaxTxSpeed(zes_fabric_port_speed_t &maxTxSpeed) {
    ::memset(&maxTxSpeed, '\0', sizeof(maxTxSpeed));
}

WddmFabricPortImp::WddmFabricPortImp(OsFabricDevice *pOsFabricDevice, uint32_t portNum) {
}

WddmFabricPortImp::~WddmFabricPortImp() {
}

OsFabricDevice *OsFabricDevice::create(OsSysman *pOsSysman) {
    WddmFabricDeviceImp *pWddmFabricDeviceImp = new WddmFabricDeviceImp(pOsSysman);
    return pWddmFabricDeviceImp;
}

OsFabricPort *OsFabricPort::create(OsFabricDevice *pOsFabricDevice, uint32_t portNum) {
    WddmFabricPortImp *pWddmFabricPortImp = new WddmFabricPortImp(pOsFabricDevice, portNum);
    return pWddmFabricPortImp;
}

} // namespace L0
