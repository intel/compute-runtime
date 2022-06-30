/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "os_fabric_port_imp_prelim.h"

#include "shared/source/helpers/debug_helpers.h"

#include <cstdio>

namespace L0 {

uint32_t LinuxFabricDeviceImp::getNumPorts() {
    pFabricDeviceAccess->getPorts(portIds);
    return static_cast<uint32_t>(portIds.size());
}

void LinuxFabricDeviceImp::getPortId(uint32_t portNumber, zes_fabric_port_id_t &portId) {
    UNRECOVERABLE_IF(getNumPorts() <= portNumber);
    portId = portIds[portNumber];
}

void LinuxFabricDeviceImp::getProperties(const zes_fabric_port_id_t portId, std::string &model, bool &onSubdevice,
                                         uint32_t &subdeviceId, zes_fabric_port_speed_t &maxRxSpeed, zes_fabric_port_speed_t &maxTxSpeed) {
    pFabricDeviceAccess->getProperties(portId, model, onSubdevice, subdeviceId, maxRxSpeed, maxTxSpeed);
}

ze_result_t LinuxFabricDeviceImp::getState(const zes_fabric_port_id_t portId, zes_fabric_port_state_t *pState) {
    return pFabricDeviceAccess->getState(portId, *pState);
}

ze_result_t LinuxFabricDeviceImp::getThroughput(const zes_fabric_port_id_t portId, zes_fabric_port_throughput_t *pThroughput) {
    return pFabricDeviceAccess->getThroughput(portId, *pThroughput);
}

ze_result_t LinuxFabricDeviceImp::performSweep() {
    uint32_t start = 0U;
    uint32_t end = 0U;
    ze_result_t result = ZE_RESULT_SUCCESS;

    result = forceSweep();
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    result = routingQuery(start, end);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    while (end < start) {
        uint32_t newStart;
        result = routingQuery(newStart, end);
        if (ZE_RESULT_SUCCESS != result) {
            return result;
        }
    }
    return result;
}

ze_result_t LinuxFabricDeviceImp::getPortEnabledState(const zes_fabric_port_id_t portId, bool &enabled) {
    return pFabricDeviceAccess->getPortEnabledState(portId, enabled);
}

ze_result_t LinuxFabricDeviceImp::enablePort(const zes_fabric_port_id_t portId) {
    ze_result_t result = enable(portId);
    // usage should be enabled, but make sure in case of previous errors
    enableUsage(portId);
    return result;
}

ze_result_t LinuxFabricDeviceImp::disablePort(const zes_fabric_port_id_t portId) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    result = disableUsage(portId);
    if (ZE_RESULT_SUCCESS == result) {
        result = disable(portId);
        if (ZE_RESULT_SUCCESS == result) {
            return enableUsage(portId);
        }
    }
    // Try not so leave port usage disabled on an error
    enableUsage(portId);
    return result;
}

ze_result_t LinuxFabricDeviceImp::getPortBeaconState(const zes_fabric_port_id_t portId, bool &enabled) {
    return pFabricDeviceAccess->getPortBeaconState(portId, enabled);
}

ze_result_t LinuxFabricDeviceImp::enablePortBeaconing(const zes_fabric_port_id_t portId) {
    return pFabricDeviceAccess->enablePortBeaconing(portId);
}

ze_result_t LinuxFabricDeviceImp::disablePortBeaconing(const zes_fabric_port_id_t portId) {
    return pFabricDeviceAccess->disablePortBeaconing(portId);
}

ze_result_t LinuxFabricDeviceImp::enable(const zes_fabric_port_id_t portId) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    result = pFabricDeviceAccess->enable(portId);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    return performSweep();
}

ze_result_t LinuxFabricDeviceImp::disable(const zes_fabric_port_id_t portId) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    result = pFabricDeviceAccess->disable(portId);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    return performSweep();
}

ze_result_t LinuxFabricDeviceImp::enableUsage(const zes_fabric_port_id_t portId) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    result = pFabricDeviceAccess->enableUsage(portId);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    return performSweep();
}

ze_result_t LinuxFabricDeviceImp::disableUsage(const zes_fabric_port_id_t portId) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    result = pFabricDeviceAccess->disableUsage(portId);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    return performSweep();
}

ze_result_t LinuxFabricDeviceImp::forceSweep() {
    return pFabricDeviceAccess->forceSweep();
}

ze_result_t LinuxFabricDeviceImp::routingQuery(uint32_t &start, uint32_t &end) {
    return pFabricDeviceAccess->routingQuery(start, end);
}

LinuxFabricDeviceImp::LinuxFabricDeviceImp(OsSysman *pOsSysman) {
    pFabricDeviceAccess = FabricDeviceAccess::create(pOsSysman);
    UNRECOVERABLE_IF(nullptr == pFabricDeviceAccess);
}

LinuxFabricDeviceImp::~LinuxFabricDeviceImp() {
    delete pFabricDeviceAccess;
}

ze_result_t LinuxFabricPortImp::getLinkType(zes_fabric_link_type_t *pLinkType) {
    ::snprintf(pLinkType->desc, ZES_MAX_FABRIC_LINK_TYPE_SIZE, "%s", "XeLink");
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFabricPortImp::getConfig(zes_fabric_port_config_t *pConfig) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    bool enabled = false;
    result = pLinuxFabricDeviceImp->getPortEnabledState(portId, enabled);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    pConfig->enabled = enabled == true;
    result = pLinuxFabricDeviceImp->getPortBeaconState(portId, enabled);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    pConfig->beaconing = enabled == true;
    return result;
}

ze_result_t LinuxFabricPortImp::setConfig(const zes_fabric_port_config_t *pConfig) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    bool enabled = false;
    result = pLinuxFabricDeviceImp->getPortEnabledState(portId, enabled);
    if (ZE_RESULT_SUCCESS == result && enabled != pConfig->enabled) {
        if (pConfig->enabled) {
            result = pLinuxFabricDeviceImp->enablePort(portId);
        } else {
            result = pLinuxFabricDeviceImp->disablePort(portId);
        }
    }
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    bool beaconing = false;
    result = pLinuxFabricDeviceImp->getPortBeaconState(portId, beaconing);
    if (ZE_RESULT_SUCCESS == result && beaconing != pConfig->beaconing) {
        if (pConfig->beaconing) {
            result = pLinuxFabricDeviceImp->enablePortBeaconing(portId);
        } else {
            result = pLinuxFabricDeviceImp->disablePortBeaconing(portId);
        }
    }
    return result;
}

ze_result_t LinuxFabricPortImp::getState(zes_fabric_port_state_t *pState) {
    return pLinuxFabricDeviceImp->getState(portId, pState);
}

ze_result_t LinuxFabricPortImp::getThroughput(zes_fabric_port_throughput_t *pThroughput) {
    return pLinuxFabricDeviceImp->getThroughput(portId, pThroughput);
}

ze_result_t LinuxFabricPortImp::getProperties(zes_fabric_port_properties_t *pProperties) {
    ::snprintf(pProperties->model, ZES_MAX_FABRIC_PORT_MODEL_SIZE, "%s", this->model.c_str());
    pProperties->onSubdevice = this->onSubdevice;
    pProperties->subdeviceId = this->subdeviceId;
    pProperties->portId = this->portId;
    pProperties->maxRxSpeed = this->maxRxSpeed;
    pProperties->maxTxSpeed = this->maxTxSpeed;
    return ZE_RESULT_SUCCESS;
}

LinuxFabricPortImp::LinuxFabricPortImp(OsFabricDevice *pOsFabricDevice, uint32_t portNum) {
    pLinuxFabricDeviceImp = static_cast<LinuxFabricDeviceImp *>(pOsFabricDevice);
    this->portNum = portNum;
    pLinuxFabricDeviceImp->getPortId(this->portNum, this->portId);
    pLinuxFabricDeviceImp->getProperties(this->portId, this->model, this->onSubdevice, this->subdeviceId, this->maxRxSpeed, this->maxTxSpeed);
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
