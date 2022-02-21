/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "fabric_device_access_imp.h"

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

#include <limits>

namespace L0 {

const std::string iafPath = "device/";
const std::string iafDirectory = "iaf.";
const std::string fabricIdFile = "/iaf_fabric_id";

ze_result_t FabricDeviceAccessNl::getState(const zes_fabric_port_id_t portId, zes_fabric_port_state_t &state) {
    ze_result_t result = pIafNlApi->fPortStatusQuery(portId, state);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    uint64_t guid;
    uint8_t portNumber;
    zes_fabric_port_speed_t maxRxSpeed;
    zes_fabric_port_speed_t maxTxSpeed;

    result = pIafNlApi->fportProperties(portId, guid, portNumber, maxRxSpeed, maxTxSpeed, state.rxSpeed, state.txSpeed);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
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
    return pIafNlApi->getThroughput(portId, througput);
}

ze_result_t FabricDeviceAccessNl::getPortEnabledState(const zes_fabric_port_id_t portId, bool &enabled) {
    return pIafNlApi->portStateQuery(portId, enabled);
}

ze_result_t FabricDeviceAccessNl::getPortBeaconState(const zes_fabric_port_id_t portId, bool &enabled) {
    return pIafNlApi->portBeaconStateQuery(portId, enabled);
}

ze_result_t FabricDeviceAccessNl::enablePortBeaconing(const zes_fabric_port_id_t portId) {
    return pIafNlApi->portBeaconEnable(portId);
}

ze_result_t FabricDeviceAccessNl::disablePortBeaconing(const zes_fabric_port_id_t portId) {
    return pIafNlApi->portBeaconDisable(portId);
}

ze_result_t FabricDeviceAccessNl::enable(const zes_fabric_port_id_t portId) {
    return pIafNlApi->portEnable(portId);
}

ze_result_t FabricDeviceAccessNl::disable(const zes_fabric_port_id_t portId) {
    return pIafNlApi->portDisable(portId);
}

ze_result_t FabricDeviceAccessNl::enableUsage(const zes_fabric_port_id_t portId) {
    return pIafNlApi->portUsageEnable(portId);
}

ze_result_t FabricDeviceAccessNl::disableUsage(const zes_fabric_port_id_t portId) {
    return pIafNlApi->portUsageDisable(portId);
}

ze_result_t FabricDeviceAccessNl::forceSweep() {
    return pIafNlApi->remRequest();
}

ze_result_t FabricDeviceAccessNl::routingQuery(uint32_t &start, uint32_t &end) {
    return pIafNlApi->routingGenQuery(start, end);
}

ze_result_t FabricDeviceAccessNl::getPorts(std::vector<zes_fabric_port_id_t> &ports) {
    ze_result_t result;
    result = init();
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    ports.clear();
    for (auto port : myPorts) {
        ports.push_back(port.portId);
    }
    return ZE_RESULT_SUCCESS;
}

void FabricDeviceAccessNl::getProperties(const zes_fabric_port_id_t portId, std::string &model, bool &onSubdevice,
                                         uint32_t &subdeviceId, zes_fabric_port_speed_t &maxRxSpeed, zes_fabric_port_speed_t &maxTxSpeed) {
    for (auto port : myPorts) {
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

ze_result_t FabricDeviceAccessNl::getPortSpeeds(const zes_fabric_port_id_t portId, zes_fabric_port_speed_t &maxRxSpeed, zes_fabric_port_speed_t &maxTxSpeed) {
    uint64_t guid;
    uint8_t portNumber;
    zes_fabric_port_speed_t rxSpeed;
    zes_fabric_port_speed_t txSpeed;

    return pIafNlApi->fportProperties(portId, guid, portNumber, maxRxSpeed, maxTxSpeed, rxSpeed, txSpeed);
}

ze_result_t FabricDeviceAccessNl::initMyPorts(const uint32_t fabricId) {
    uint32_t numSubdevices;

    if (ZE_RESULT_SUCCESS != getNumSubdevices(fabricId, numSubdevices)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    for (uint32_t subdevice = 0; subdevice < numSubdevices; subdevice++) {
        uint64_t guid;
        std::vector<uint8_t> ports;

        if (ZE_RESULT_SUCCESS != getSubdevice(fabricId, subdevice, guid, ports)) {
            myPorts.clear();
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        for (auto port : ports) {
            Port p;
            p.onSubdevice = numSubdevices > 1;
            p.portId.fabricId = fabricId;
            p.portId.attachId = subdevice;
            p.portId.portNumber = port;
            p.model = "XeLink";
            if (ZE_RESULT_SUCCESS != getPortSpeeds(p.portId, p.maxRxSpeed, p.maxTxSpeed)) {
                myPorts.clear();
                return ZE_RESULT_ERROR_UNKNOWN;
            }
            myPorts.push_back(p);
        }
    }
    return ZE_RESULT_SUCCESS;
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

ze_result_t FabricDeviceAccessNl::init() {
    if (myPorts.empty()) {
        std::string path;
        path.clear();
        std::vector<std::string> list;
        if (ZE_RESULT_SUCCESS != pLinuxSysmanImp->getSysfsAccess().scanDirEntries(iafPath, list)) {
            // There should be a device directory
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        for (auto entry : list) {
            if (!iafDirectory.compare(entry.substr(0, iafDirectory.length()))) {
                // device/iaf.X/iaf_fabric_id, where X is the hardware slot number
                path = iafPath + entry + fabricIdFile;
            }
        }
        if (path.empty()) {
            // This device does not have a fabric
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        std::string fabricIdStr;
        fabricIdStr.clear();
        if (ZE_RESULT_SUCCESS != pLinuxSysmanImp->getSysfsAccess().read(path, fabricIdStr)) {
            // This device has a fabric, but the iaf module isn't running
            return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
        }
        unsigned long myFabricId = 0UL;
        size_t end = 0;
        myFabricId = std::stoul(fabricIdStr, &end, 16);
        if (fabricIdStr.length() != end || myFabricId > std::numeric_limits<uint32_t>::max()) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        if (ZE_RESULT_SUCCESS != initMyPorts(static_cast<uint32_t>(myFabricId))) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }
    return ZE_RESULT_SUCCESS;
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
