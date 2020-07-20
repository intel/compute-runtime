/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fabric_port/linux/os_fabric_port_imp.h"

#include <cstdio>

namespace L0 {

uint32_t LinuxFabricDeviceImp::getNumPorts() {
    return numPorts;
}

LinuxFabricDeviceImp::LinuxFabricDeviceImp(OsSysman *pOsSysman) {
}

LinuxFabricDeviceImp::~LinuxFabricDeviceImp() {
}

ze_result_t LinuxFabricPortImp::getLinkType(zes_fabric_link_type_t *pLinkType) {
    ::snprintf(pLinkType->desc, ZET_MAX_FABRIC_LINK_TYPE_SIZE, "%s", "SAMPLE LINK, VERBOSE");
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFabricPortImp::getConfig(zes_fabric_port_config_t *pConfig) {
    *pConfig = config;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFabricPortImp::setConfig(const zes_fabric_port_config_t *pConfig) {
    config = *pConfig;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFabricPortImp::getState(zes_fabric_port_state_t *pState) {
    pState->status = ZES_FABRIC_PORT_STATUS_UNKNOWN;
    pState->qualityIssues = 0U;
    pState->failureReasons = 0U;
    pState->remotePortId.fabricId = 0U;
    pState->remotePortId.attachId = 0U;
    pState->remotePortId.portNumber = 0U;
    pState->rxSpeed.bitRate = 0LU;
    pState->rxSpeed.width = 0U;
    pState->txSpeed.bitRate = 0LU;
    pState->txSpeed.width = 0U;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFabricPortImp::getThroughput(zes_fabric_port_throughput_t *pThroughput) {
    pThroughput->rxCounter = 0LU;
    pThroughput->txCounter = 0LU;
    return ZE_RESULT_SUCCESS;
}

void LinuxFabricPortImp::getModel(char *model) {
    ::snprintf(model, ZET_MAX_FABRIC_PORT_MODEL_SIZE, "%s", this->model.c_str());
}

void LinuxFabricPortImp::getPortId(zes_fabric_port_id_t &portId) {
    portId = this->portId;
}

void LinuxFabricPortImp::getMaxRxSpeed(zes_fabric_port_speed_t &maxRxSpeed) {
    maxRxSpeed = this->maxRxSpeed;
}

void LinuxFabricPortImp::getMaxTxSpeed(zes_fabric_port_speed_t &maxTxSpeed) {
    maxTxSpeed = this->maxTxSpeed;
}

ze_result_t LinuxFabricPortImp::getLinkType(ze_bool_t verbose, zet_fabric_link_type_t *pLinkType) {
    if (verbose) {
        ::snprintf(reinterpret_cast<char *>(pLinkType->desc), ZET_MAX_FABRIC_LINK_TYPE_SIZE, "%s", "SAMPLE LINK, VERBOSE");
    } else {
        ::snprintf(reinterpret_cast<char *>(pLinkType->desc), ZET_MAX_FABRIC_LINK_TYPE_SIZE, "%s", "SAMPLE LINK");
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFabricPortImp::getConfig(zet_fabric_port_config_t *pConfig) {
    *pConfig = zetConfig;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFabricPortImp::setConfig(const zet_fabric_port_config_t *pConfig) {
    zetConfig = *pConfig;
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
    ::snprintf(reinterpret_cast<char *>(model), ZET_MAX_FABRIC_PORT_MODEL_SIZE, "%s", this->zetModel.c_str());
}

void LinuxFabricPortImp::getPortUuid(zet_fabric_port_uuid_t &portUuid) {
    portUuid = this->zetPortUuid;
}

void LinuxFabricPortImp::getMaxRxSpeed(zet_fabric_port_speed_t &maxRxSpeed) {
    maxRxSpeed = this->zetMaxRxSpeed;
}

void LinuxFabricPortImp::getMaxTxSpeed(zet_fabric_port_speed_t &maxTxSpeed) {
    maxTxSpeed = this->zetMaxTxSpeed;
}

LinuxFabricPortImp::LinuxFabricPortImp(OsFabricDevice *pOsFabricDevice, uint32_t portNum) {
    this->portNum = portNum;
    model = std::string("EXAMPLE");
    zetModel = std::string("EXAMPLE");
}

LinuxFabricPortImp::~LinuxFabricPortImp() {
}

OsFabricDevice *OsFabricDevice::create(OsSysman *pOsSysman) {
    LinuxFabricDeviceImp *pLinuxFabricDeviceImp = new LinuxFabricDeviceImp(pOsSysman);
    return pLinuxFabricDeviceImp;
}

OsFabricPort *OsFabricPort::create(OsFabricDevice *pOsFabricDevice, uint32_t portNum) {
    LinuxFabricPortImp *pLinuxFabricPortImp = new LinuxFabricPortImp(pOsFabricDevice, portNum);
    return pLinuxFabricPortImp;
}

} // namespace L0
