/*
 * Copyright (C) 2020-2023 Intel Corporation
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

ze_result_t LinuxFabricDeviceImp::getMultiPortThroughput(std::vector<zes_fabric_port_id_t> &portIdList, zes_fabric_port_throughput_t **pThroughput) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

LinuxFabricDeviceImp::LinuxFabricDeviceImp(OsSysman *pOsSysman) {
}

LinuxFabricDeviceImp::~LinuxFabricDeviceImp() {
}

ze_result_t LinuxFabricPortImp::getLinkType(zes_fabric_link_type_t *pLinkType) {
    ::snprintf(pLinkType->desc, ZES_MAX_FABRIC_LINK_TYPE_SIZE, "%s", "SAMPLE LINK, VERBOSE");
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

ze_result_t LinuxFabricPortImp::getErrorCounters(zes_fabric_port_error_counters_t *pErrors) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFabricPortImp::getProperties(zes_fabric_port_properties_t *pProperties) {
    ::snprintf(pProperties->model, ZES_MAX_FABRIC_PORT_MODEL_SIZE, "%s", this->model.c_str());
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0U;
    pProperties->portId = this->portId;
    pProperties->maxRxSpeed = this->maxRxSpeed;
    pProperties->maxTxSpeed = this->maxTxSpeed;
    return ZE_RESULT_SUCCESS;
}

LinuxFabricPortImp::LinuxFabricPortImp(OsFabricDevice *pOsFabricDevice, uint32_t portNum) {
    this->portNum = portNum;
    model = std::string("EXAMPLE");
}

LinuxFabricPortImp::~LinuxFabricPortImp() {
}

OsFabricDevice *OsFabricDevice::create(OsSysman *pOsSysman) {
    LinuxFabricDeviceImp *pLinuxFabricDeviceImp = new LinuxFabricDeviceImp(pOsSysman);
    return pLinuxFabricDeviceImp;
}

std::unique_ptr<OsFabricPort> OsFabricPort::create(OsFabricDevice *pOsFabricDevice, uint32_t portNum) {
    std::unique_ptr<LinuxFabricPortImp> pLinuxFabricPortImp = std::make_unique<LinuxFabricPortImp>(pOsFabricDevice, portNum);
    return pLinuxFabricPortImp;
}

} // namespace L0
