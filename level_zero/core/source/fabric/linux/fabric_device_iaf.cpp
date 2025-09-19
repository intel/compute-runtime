/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/fabric/linux/fabric_device_iaf.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/fabric/fabric.h"

namespace L0 {

FabricDeviceIaf::FabricDeviceIaf(Device *device) : device(device) {

    DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    if (deviceImp->numSubDevices == 0) {
        // Add one sub-device
        subDeviceIafs.push_back(std::make_unique<FabricSubDeviceIaf>(device));
    } else {
        for (const auto &subDevice : deviceImp->subDevices) {
            subDeviceIafs.push_back(std::make_unique<FabricSubDeviceIaf>(subDevice));
        }
    }
}

ze_result_t FabricDeviceIaf::enumerate() {

    ze_result_t result = ZE_RESULT_SUCCESS;
    for (auto &subDeviceIaf : subDeviceIafs) {
        result = subDeviceIaf->enumerate();
        if (result != ZE_RESULT_SUCCESS) {
            break;
        }
    }

    return result;
}

bool FabricDeviceIaf::getEdgeProperty(FabricVertex *neighborVertex, ze_fabric_edge_exp_properties_t &edgeProperty) {

    bool isConnected = false;

    std::vector<ze_fabric_edge_exp_properties_t> subDeviceEdgeProperties = {};
    for (auto &subDeviceIaf : subDeviceIafs) {

        ze_fabric_edge_exp_properties_t subDeviceEdgeProperty = {};
        bool isSubdeviceConnected = subDeviceIaf->getEdgeProperty(neighborVertex, subDeviceEdgeProperty);
        if (isSubdeviceConnected) {
            subDeviceEdgeProperties.push_back(subDeviceEdgeProperty);
        }
    }

    if (subDeviceEdgeProperties.size() > 0) {

        ze_uuid_t &uuid = edgeProperty.uuid;
        std::memset(uuid.id, 0, ZE_MAX_UUID_SIZE);

        // Copy current device fabric and attach id
        memcpy_s(&uuid.id[0], 4, &subDeviceEdgeProperties[0].uuid.id[0], 4);
        // Use 255 (impossible tile (attach) id to identify root device)
        uuid.id[4] = 255;
        // Copy neighboring device fabric id
        memcpy_s(&uuid.id[8], 4, &subDeviceEdgeProperties[0].uuid.id[8], 4);
        memcpy_s(&uuid.id[12], 1, &subDeviceEdgeProperties[0].uuid.id[12], 1);

        uint32_t accumulatedBandwidth = 0;
        for (const auto &subEdgeProperty : subDeviceEdgeProperties) {
            accumulatedBandwidth += subEdgeProperty.bandwidth;
        }

        memcpy_s(edgeProperty.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, "XeLink", 7);
        edgeProperty.bandwidth = accumulatedBandwidth;
        edgeProperty.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
        edgeProperty.latency = subDeviceEdgeProperties[0].latency;
        edgeProperty.latencyUnit = subDeviceEdgeProperties[0].latencyUnit;
        edgeProperty.duplexity = ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX;

        isConnected = true;
    }

    return isConnected;
}

FabricSubDeviceIaf::FabricSubDeviceIaf(Device *device) : device(device) {
    pIafNlApi = std::make_unique<IafNlApi>();
    UNRECOVERABLE_IF(nullptr == pIafNlApi);
}

ze_result_t FabricSubDeviceIaf::enumerate() {

    auto &osInterface = device->getNEODevice()->getRootDeviceEnvironment().osInterface;

    if (osInterface == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    if (osInterface->getDriverModel()->getDriverModelType() != NEO::DriverModelType::drm) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    auto pDrm = osInterface->getDriverModel()->as<NEO::Drm>();
    std::optional<std::string> rootPciPath = NEO::getPciLinkPath(pDrm->getFileDescriptor());
    if (!rootPciPath.has_value()) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PCI Path not found%s\n", "");
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    std::vector<IafPort> iafPorts = {};
    ze_result_t result = pIafNlApi->getPorts("/sys/class/drm/" + rootPciPath.value() + "/device/", iafPorts);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
    uint32_t physicalSubDeviceId = deviceImp->getPhysicalSubDeviceId();

    // Remove ports which do not belong to this device
    for (auto iter = iafPorts.begin(); iter != iafPorts.end();) {
        IafPort &port = *iter;
        if (port.portId.attachId != physicalSubDeviceId) {
            iter = iafPorts.erase(iter);
            continue;
        }
        ++iter;
    }

    // Get Connections
    for (auto &iafPort : iafPorts) {
        FabricPortConnection connection = {};
        ze_result_t result = getConnection(iafPort, connection);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                  "failure observed for IafPort{0x%x, 0x%x, 0x%x}: 0x%x\n",
                                  iafPort.portId.fabricId, iafPort.portId.attachId, iafPort.portId.portNumber,
                                  result);
            continue;
        }
        connections.push_back(connection);
    }

    // Get guid
    if (iafPorts.size() > 0) {
        std::vector<uint8_t> ports = {};
        if (ZE_RESULT_SUCCESS != pIafNlApi->subdevicePropertiesGet(iafPorts[0].portId.fabricId, physicalSubDeviceId, guid, ports)) {
            connections.clear();
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                  "failure during fabric port guid reading {0x%x, 0x%x, 0x%x}: result: 0x%x subdeviceId:%d\n",
                                  iafPorts[0].portId.fabricId, iafPorts[0].portId.attachId, iafPorts[0].portId.portNumber,
                                  result, physicalSubDeviceId);
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricSubDeviceIaf::getConnection(IafPort &port, FabricPortConnection &connection) {

    IafPortState iafPortState = {};
    ze_result_t result = pIafNlApi->fPortStatusQuery(port.portId, iafPortState);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "fPortStatusQuery Unsuccessful: %s\n", result);
        return result;
    }
    if (iafPortState.healthStatus != IAF_FPORT_HEALTH_HEALTHY) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "IAF PORT not Healthy%s\n", "");
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Consider only healthy ports
    uint64_t neighbourGuid;
    uint8_t neighbourPortNumber;
    IafPortSpeed maxRxSpeed = {}, maxTxSpeed = {};
    IafPortSpeed rxSpeed = {}, txSpeed = {};

    result = pIafNlApi->fportProperties(port.portId, neighbourGuid, neighbourPortNumber, maxRxSpeed, maxTxSpeed, rxSpeed, txSpeed);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    connection.currentid = port.portId;
    connection.neighbourPortNumber = neighbourPortNumber;
    connection.neighbourGuid = neighbourGuid;
    const double bandwidthInBitsPerSecond = static_cast<double>(maxRxSpeed.bitRate * maxRxSpeed.width);
    // 8ull - bits to bytes; 1e9 - seconds to nano-seconds
    connection.bandwidthInBytesPerNanoSecond = static_cast<uint32_t>(bandwidthInBitsPerSecond / (8ull * 1e9));
    connection.isDuplex = true;
    return ZE_RESULT_SUCCESS;
}

bool FabricSubDeviceIaf::getEdgeProperty(FabricSubDeviceIaf *pNeighbourInterface,
                                         ze_fabric_edge_exp_properties_t &edgeProperty) {
    bool isConnected = false;
    uint32_t accumulatedBandwidth = 0;
    for (auto &connection : connections) {
        if (connection.neighbourGuid == pNeighbourInterface->guid) {
            accumulatedBandwidth += connection.bandwidthInBytesPerNanoSecond;
        }
    }

    if (accumulatedBandwidth != 0) {
        ze_uuid_t &uuid = edgeProperty.uuid;
        DEBUG_BREAK_IF(pNeighbourInterface->connections.size() == 0);

        std::memset(uuid.id, 0, ZE_MAX_UUID_SIZE);
        memcpy_s(&uuid.id[0], 4, &connections[0].currentid.fabricId, 4);
        memcpy_s(&uuid.id[4], 1, &connections[0].currentid.attachId, 1);

        // Considering the neighboring port is attached on a subdevice, fabricId and attachId could be used from
        // any of the connection
        auto neighbourFabricId = pNeighbourInterface->connections[0].currentid.fabricId;
        memcpy_s(&uuid.id[8], 4, &neighbourFabricId, 4);
        memcpy_s(&uuid.id[12], 1, &pNeighbourInterface->connections[0].currentid.attachId, 1);

        memcpy_s(edgeProperty.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, "XeLink", 7);
        edgeProperty.bandwidth = accumulatedBandwidth;
        edgeProperty.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;

        auto &osInterface = device->getNEODevice()->getRootDeviceEnvironment().osInterface;
        auto pDrm = osInterface->getDriverModel()->as<NEO::Drm>();

        edgeProperty.latency = std::numeric_limits<uint32_t>::max();
        edgeProperty.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
        uint32_t bandwidth = 0;
        if (pDrm->getIoctlHelper()->getFabricLatency(neighbourFabricId, edgeProperty.latency, bandwidth) == true) {
            edgeProperty.latencyUnit = ZE_LATENCY_UNIT_HOP;
        }

        edgeProperty.duplexity = ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX;
        isConnected = true;
    }

    return isConnected;
}

bool FabricSubDeviceIaf::getEdgeProperty(FabricVertex *neighborVertex,
                                         ze_fabric_edge_exp_properties_t &edgeProperty) {

    ze_uuid_t &uuid = edgeProperty.uuid;
    bool isConnected = false;
    uint32_t accumulatedBandwidth = 0;
    DeviceImp *neighborDeviceImp = static_cast<DeviceImp *>(neighborVertex->device);

    // If the neighbor is a root device
    if (neighborDeviceImp->isSubdevice == false) {

        std::vector<ze_fabric_edge_exp_properties_t> subEdgeProperties = {};

        FabricDeviceIaf *deviceIaf = static_cast<FabricDeviceIaf *>(neighborVertex->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());
        // Get the edges to the neighbors subVertices
        for (auto &subDeviceIaf : deviceIaf->subDeviceIafs) {
            ze_fabric_edge_exp_properties_t edgeProperty = {};
            bool subDevicesConnected = getEdgeProperty(subDeviceIaf.get(), edgeProperty);
            if (subDevicesConnected) {
                subEdgeProperties.push_back(edgeProperty);
            }
        }

        // Add an edge for this subVertex to the neighbor root
        if (subEdgeProperties.size() > 0) {

            std::memset(uuid.id, 0, ZE_MAX_UUID_SIZE);
            // Copy current device fabric and attach id
            memcpy_s(&uuid.id[0], 4, &subEdgeProperties[0].uuid.id[0], 4);
            memcpy_s(&uuid.id[4], 1, &subEdgeProperties[0].uuid.id[4], 1);
            // Copy neighboring device fabric id
            memcpy_s(&uuid.id[8], 4, &subEdgeProperties[0].uuid.id[8], 4);
            // Use 255 (impossible tile (attach) id to identify root device)
            uuid.id[12] = 255;

            for (const auto &subEdgeProperty : subEdgeProperties) {
                accumulatedBandwidth += subEdgeProperty.bandwidth;
            }

            memcpy_s(edgeProperty.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, "XeLink", 7);
            edgeProperty.bandwidth = accumulatedBandwidth;
            edgeProperty.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
            edgeProperty.latency = subEdgeProperties[0].latency;
            edgeProperty.latencyUnit = subEdgeProperties[0].latencyUnit;
            edgeProperty.duplexity = ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX;

            isConnected = true;
        }
    } else {
        FabricSubDeviceIaf *pNeighbourInterface =
            static_cast<FabricSubDeviceIaf *>(neighborVertex->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());
        isConnected = getEdgeProperty(pNeighbourInterface, edgeProperty);
    }

    return isConnected;
}

void FabricSubDeviceIaf::setIafNlApi(IafNlApi *iafNlApi) {
    pIafNlApi.reset(iafNlApi);
}

std::unique_ptr<FabricDeviceInterface> FabricDeviceInterface::createFabricDeviceInterfaceIaf(const FabricVertex *fabricVertex) {

    DeviceImp *deviceImp = static_cast<DeviceImp *>(fabricVertex->device);

    if (deviceImp->isSubdevice) {
        return std::unique_ptr<FabricDeviceInterface>(new (std::nothrow) FabricSubDeviceIaf(fabricVertex->device));
    }

    return std::unique_ptr<FabricDeviceInterface>(new (std::nothrow) FabricDeviceIaf(fabricVertex->device));
}

} // namespace L0
