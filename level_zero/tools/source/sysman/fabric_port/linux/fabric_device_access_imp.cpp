/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "fabric_device_access_imp.h"

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

#include <limits>

namespace L0 {

void FabricDeviceAccessNl::readIafPortStatus(zes_fabric_port_state_t &state, const IafPortState &iafPortState) {

    state.failureReasons = 0;
    state.qualityIssues = 0;
    switch (iafPortState.healthStatus) {
    case IAF_FPORT_HEALTH_OFF:
        state.status = ZES_FABRIC_PORT_STATUS_DISABLED;
        break;
    case IAF_FPORT_HEALTH_FAILED:
        state.status = ZES_FABRIC_PORT_STATUS_FAILED;
        if (1 == iafPortState.failed || 1 == iafPortState.isolated || 1 == iafPortState.linkDown) {
            state.failureReasons |= ZES_FABRIC_PORT_FAILURE_FLAG_FAILED;
        }
        if (1 == iafPortState.didNotTrain) {
            state.failureReasons |= ZES_FABRIC_PORT_FAILURE_FLAG_TRAINING_TIMEOUT;
        }
        if (1 == iafPortState.flapping) {
            state.failureReasons |= ZES_FABRIC_PORT_FAILURE_FLAG_FLAPPING;
        }
        break;
    case IAF_FPORT_HEALTH_DEGRADED:
        state.status = ZES_FABRIC_PORT_STATUS_DEGRADED;
        if (1 == iafPortState.lqi) {
            state.qualityIssues |= ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_LINK_ERRORS;
        }
        if (1 == iafPortState.lwd || 1 == iafPortState.rate) {
            state.qualityIssues |= ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_SPEED;
        }
        break;
    case IAF_FPORT_HEALTH_HEALTHY:
        state.status = ZES_FABRIC_PORT_STATUS_HEALTHY;
        break;
    default:
        state.status = ZES_FABRIC_PORT_STATUS_UNKNOWN;
        break;
    }
}

ze_result_t FabricDeviceAccessNl::getState(const zes_fabric_port_id_t portId, zes_fabric_port_state_t &state) {
    IafPortState iafPortState = {};
    const IafPortId iafPortId(portId.fabricId, portId.attachId, portId.portNumber);
    ze_result_t result = pIafNlApi->fPortStatusQuery(iafPortId, iafPortState);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    readIafPortStatus(state, iafPortState);

    uint64_t guid;
    uint8_t portNumber;
    IafPortSpeed maxRxSpeed = {};
    IafPortSpeed maxTxSpeed = {};
    IafPortSpeed rxSpeed = {};
    IafPortSpeed txSpeed = {};

    result = pIafNlApi->fportProperties(iafPortId, guid, portNumber, maxRxSpeed, maxTxSpeed, rxSpeed, txSpeed);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    readIafPortSpeed(state.rxSpeed, rxSpeed);
    readIafPortSpeed(state.txSpeed, txSpeed);

    switch (state.status) {
    case ZES_FABRIC_PORT_STATUS_HEALTHY:
    case ZES_FABRIC_PORT_STATUS_DEGRADED:
    case ZES_FABRIC_PORT_STATUS_FAILED: {
        auto it = guidMap.find(guid);

        if (guidMap.end() == it) {
            populateGuidMap();
            it = guidMap.find(guid);
        }
        if (guidMap.end() != it) {
            state.remotePortId = it->second;
            state.remotePortId.portNumber = portNumber;
        }
    } break;
    default:
        break;
    }
    return result;
}

ze_result_t FabricDeviceAccessNl::getThroughput(const zes_fabric_port_id_t portId, zes_fabric_port_throughput_t &througput) {
    const IafPortId iafPortId(portId.fabricId, portId.attachId, portId.portNumber);
    IafPortThroughPut iafThroughPut = {};
    ze_result_t result = pIafNlApi->getThroughput(iafPortId, iafThroughPut);
    readIafPortThroughPut(througput, iafThroughPut);
    return result;
}

ze_result_t FabricDeviceAccessNl::getPortEnabledState(const zes_fabric_port_id_t portId, bool &enabled) {
    const IafPortId iafPortId(portId.fabricId, portId.attachId, portId.portNumber);
    return pIafNlApi->portStateQuery(iafPortId, enabled);
}

ze_result_t FabricDeviceAccessNl::getPortBeaconState(const zes_fabric_port_id_t portId, bool &enabled) {
    const IafPortId iafPortId(portId.fabricId, portId.attachId, portId.portNumber);
    return pIafNlApi->portBeaconStateQuery(iafPortId, enabled);
}

ze_result_t FabricDeviceAccessNl::enablePortBeaconing(const zes_fabric_port_id_t portId) {
    const IafPortId iafPortId(portId.fabricId, portId.attachId, portId.portNumber);
    return pIafNlApi->portBeaconEnable(iafPortId);
}

ze_result_t FabricDeviceAccessNl::disablePortBeaconing(const zes_fabric_port_id_t portId) {
    const IafPortId iafPortId(portId.fabricId, portId.attachId, portId.portNumber);
    return pIafNlApi->portBeaconDisable(iafPortId);
}

ze_result_t FabricDeviceAccessNl::enable(const zes_fabric_port_id_t portId) {
    const IafPortId iafPortId(portId.fabricId, portId.attachId, portId.portNumber);
    return pIafNlApi->portEnable(iafPortId);
}

ze_result_t FabricDeviceAccessNl::disable(const zes_fabric_port_id_t portId) {
    const IafPortId iafPortId(portId.fabricId, portId.attachId, portId.portNumber);
    return pIafNlApi->portDisable(iafPortId);
}

ze_result_t FabricDeviceAccessNl::enableUsage(const zes_fabric_port_id_t portId) {
    const IafPortId iafPortId(portId.fabricId, portId.attachId, portId.portNumber);
    return pIafNlApi->portUsageEnable(iafPortId);
}

ze_result_t FabricDeviceAccessNl::disableUsage(const zes_fabric_port_id_t portId) {
    const IafPortId iafPortId(portId.fabricId, portId.attachId, portId.portNumber);
    return pIafNlApi->portUsageDisable(iafPortId);
}

ze_result_t FabricDeviceAccessNl::forceSweep() {
    return pIafNlApi->remRequest();
}

ze_result_t FabricDeviceAccessNl::routingQuery(uint32_t &start, uint32_t &end) {
    return pIafNlApi->routingGenQuery(start, end);
}

ze_result_t FabricDeviceAccessNl::getPorts(std::vector<zes_fabric_port_id_t> &ports) {

    std::vector<IafPort> iafPorts = {};
    std::string iafRealPath = {};
    pLinuxSysmanImp->getSysfsAccess().getRealPath(iafPath, iafRealPath);
    ze_result_t result = pIafNlApi->getPorts(iafRealPath, iafPorts);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    // Update fabricPorts
    for (const auto &iafPort : iafPorts) {
        Port port = {};
        readIafPort(port, iafPort);
        fabricPorts.push_back(port);
    }

    ports.clear();
    for (auto port : fabricPorts) {
        ports.push_back(port.portId);
    }
    return ZE_RESULT_SUCCESS;
}

void FabricDeviceAccessNl::getProperties(const zes_fabric_port_id_t portId, std::string &model, bool &onSubdevice,
                                         uint32_t &subdeviceId, zes_fabric_port_speed_t &maxRxSpeed, zes_fabric_port_speed_t &maxTxSpeed) {
    for (auto port : fabricPorts) {
        UNRECOVERABLE_IF(portId.fabricId != port.portId.fabricId);
        if (portId.attachId == port.portId.attachId && portId.portNumber == port.portId.portNumber) {
            model = port.model;
            onSubdevice = port.onSubdevice;
            subdeviceId = port.portId.attachId;
            maxRxSpeed = port.maxRxSpeed;
            maxTxSpeed = port.maxTxSpeed;
            return;
        }
    }
}

ze_result_t FabricDeviceAccessNl::getAllFabricIds(std::vector<uint32_t> &fabricIds) {
    return pIafNlApi->deviceEnum(fabricIds);
}

ze_result_t FabricDeviceAccessNl::getNumSubdevices(const uint32_t fabricId, uint32_t &numSubdevices) {
    return pIafNlApi->fabricDeviceProperties(fabricId, numSubdevices);
}

ze_result_t FabricDeviceAccessNl::getSubdevice(const uint32_t fabricId, const uint32_t subdevice, uint64_t &guid, std::vector<uint8_t> &ports) {
    return pIafNlApi->subdevicePropertiesGet(fabricId, subdevice, guid, ports);
}

void FabricDeviceAccessNl::populateGuidMap() {
    std::vector<uint32_t> fabricIds;

    if (ZE_RESULT_SUCCESS != getAllFabricIds(fabricIds)) {
        return;
    }
    for (auto fabricId : fabricIds) {
        uint32_t numSubdevices = 0;

        if (ZE_RESULT_SUCCESS != getNumSubdevices(fabricId, numSubdevices)) {
            return;
        }
        for (uint32_t subdevice = 0; subdevice < numSubdevices; subdevice++) {
            uint64_t guid;
            std::vector<uint8_t> ports;

            if (ZE_RESULT_SUCCESS != getSubdevice(fabricId, subdevice, guid, ports)) {
                return;
            }
            zes_fabric_port_id_t portId;

            portId.fabricId = fabricId;
            portId.attachId = subdevice;
            portId.portNumber = ports.size();
            guidMap[guid] = portId;
        }
    }
    return;
}

FabricDeviceAccessNl::FabricDeviceAccessNl(OsSysman *pOsSysman) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pIafNlApi = new IafNlApi;
    UNRECOVERABLE_IF(nullptr == pIafNlApi);
}

FabricDeviceAccessNl::~FabricDeviceAccessNl() {
    if (nullptr != pIafNlApi) {
        delete pIafNlApi;
        pIafNlApi = nullptr;
    }
}

FabricDeviceAccess *FabricDeviceAccess::create(OsSysman *pOsSysman) {
    return new FabricDeviceAccessNl(pOsSysman);
}

} // namespace L0
