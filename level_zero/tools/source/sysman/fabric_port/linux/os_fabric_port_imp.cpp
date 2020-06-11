/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fabric_port/linux/os_fabric_port_imp.h"

#include <cstdio>

namespace L0 {

ze_result_t LinuxFabricPortImp::getLinkType(ze_bool_t verbose, zet_fabric_link_type_t *pLinkType) {
    if (verbose) {
        ::snprintf(reinterpret_cast<char *>(pLinkType->desc), ZET_MAX_FABRIC_LINK_TYPE_SIZE, "%s", "SAMPLE LINK, VERBOSE");
    } else {
        ::snprintf(reinterpret_cast<char *>(pLinkType->desc), ZET_MAX_FABRIC_LINK_TYPE_SIZE, "%s", "SAMPLE LINK");
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFabricPortImp::getConfig(zet_fabric_port_config_t *pConfig) {
    *pConfig = config;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFabricPortImp::setConfig(const zet_fabric_port_config_t *pConfig) {
    config = *pConfig;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFabricPortImp::getState(zet_fabric_port_state_t *pState) {
    pState->status = ZET_FABRIC_PORT_STATUS_BLACK;
    pState->qualityIssues = ZET_FABRIC_PORT_QUAL_ISSUES_NONE;
    pState->stabilityIssues = ZET_FABRIC_PORT_STAB_ISSUES_NONE;
    pState->rxSpeed.bitRate = 0LU;
    pState->rxSpeed.width = 0U;
    pState->rxSpeed.maxBandwidth = 0LU;
    pState->txSpeed.bitRate = 0LU;
    pState->txSpeed.width = 0U;
    pState->txSpeed.maxBandwidth = 0LU;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFabricPortImp::getThroughput(zet_fabric_port_throughput_t *pThroughput) {
    pThroughput->rxCounter = 0LU;
    pThroughput->txCounter = 0LU;
    pThroughput->rxMaxBandwidth = 0LU;
    pThroughput->txMaxBandwidth = 0LU;
    return ZE_RESULT_SUCCESS;
}

void LinuxFabricPortImp::getModel(int8_t *model) {
    ::snprintf(reinterpret_cast<char *>(model), ZET_MAX_FABRIC_PORT_MODEL_SIZE, "%s", this->model.c_str());
}

void LinuxFabricPortImp::getPortUuid(zet_fabric_port_uuid_t &portUuid) {
    portUuid = this->portUuid;
}

void LinuxFabricPortImp::getMaxRxSpeed(zet_fabric_port_speed_t &maxRxSpeed) {
    maxRxSpeed = this->maxRxSpeed;
}

void LinuxFabricPortImp::getMaxTxSpeed(zet_fabric_port_speed_t &maxTxSpeed) {
    maxTxSpeed = this->maxTxSpeed;
}

LinuxFabricPortImp::LinuxFabricPortImp(OsSysman *pOsSysman, uint32_t portNum) {
    this->portNum = portNum;
    model = std::string("EXAMPLE");
}

LinuxFabricPortImp::~LinuxFabricPortImp() {
}

OsFabricPort *OsFabricPort::create(OsSysman *pOsSysman, uint32_t portNum) {
    LinuxFabricPortImp *pLinuxFabricPortImp = new LinuxFabricPortImp(pOsSysman, portNum);
    return static_cast<OsFabricPort *>(pLinuxFabricPortImp);
}

uint32_t OsFabricPort::numPorts(OsSysman *pOsSysman) {
    return 0U;
}

} // namespace L0
