/*
 * Copyright (C) 2020-2023 Intel Corporation
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

ze_result_t WddmFabricDeviceImp::getMultiPortThroughput(std::vector<zes_fabric_port_id_t> &portIdList, zes_fabric_port_throughput_t **pThroughput) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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

ze_result_t WddmFabricPortImp::getErrorCounters(zes_fabric_port_error_counters_t *pErrors) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFabricPortImp::getProperties(zes_fabric_port_properties_t *pProperties) {
    ::memset(pProperties->model, '\0', ZES_MAX_FABRIC_PORT_MODEL_SIZE);
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0U;
    ::memset(&pProperties->portId, '\0', sizeof(pProperties->portId));
    ::memset(&pProperties->maxRxSpeed, '\0', sizeof(pProperties->maxRxSpeed));
    ::memset(&pProperties->maxTxSpeed, '\0', sizeof(pProperties->maxTxSpeed));
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

WddmFabricPortImp::WddmFabricPortImp(OsFabricDevice *pOsFabricDevice, uint32_t portNum) {
}

WddmFabricPortImp::~WddmFabricPortImp() {
}

OsFabricDevice *OsFabricDevice::create(OsSysman *pOsSysman) {
    WddmFabricDeviceImp *pWddmFabricDeviceImp = new WddmFabricDeviceImp(pOsSysman);
    return pWddmFabricDeviceImp;
}

std::unique_ptr<OsFabricPort> OsFabricPort::create(OsFabricDevice *pOsFabricDevice, uint32_t portNum) {
    std::unique_ptr<WddmFabricPortImp> pWddmFabricPortImp = std::make_unique<WddmFabricPortImp>(pOsFabricDevice, portNum);
    return pWddmFabricPortImp;
}

} // namespace L0
