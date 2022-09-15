/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/fabric/linux/fabric_device_iaf.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/fabric/fabric.h"

namespace L0 {

FabricDeviceIaf::FabricDeviceIaf(Device *device) : device(device) {

    DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    if (deviceImp->numSubDevices == 0) {
        //Add one sub-device
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

FabricSubDeviceIaf::FabricSubDeviceIaf(Device *device) : device(device) {
    pIafNlApi = std::make_unique<IafNlApi>();
    UNRECOVERABLE_IF(nullptr == pIafNlApi);
}

ze_result_t FabricSubDeviceIaf::enumerate() {

    auto &osInterface = device->getNEODevice()->getRootDeviceEnvironment().osInterface;

    if (osInterface == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    if (osInterface->getDriverModel()->getDriverModelType() != NEO::DriverModelType::DRM) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    auto pDrm = osInterface->getDriverModel()->as<NEO::Drm>();
    std::optional<std::string> rootPciPath = NEO::getPciLinkPath(pDrm->getFileDescriptor());
    if (!rootPciPath.has_value()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    std::vector<IafPort> iafPorts = {};
    ze_result_t result = pIafNlApi->getPorts("/sys/class/drm/" + rootPciPath.value() + "/device/", iafPorts);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
    uint32_t physicalSubDeviceId = deviceImp->getPhysicalSubDeviceId();

    // Remove ports which donot belong to this device
    for (auto iter = iafPorts.begin(); iter != iafPorts.end();) {
        IafPort &port = *iter;
        if (port.portId.attachId != physicalSubDeviceId) {
            iter = iafPorts.erase(iter);
            continue;
        }
        ++iter;
    }

    // Get Connections
    for (auto iafPort : iafPorts) {
        FabricPortConnection connection = {};
        ze_result_t result = getConnection(iafPort, connection);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
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
            NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
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
    if (ZE_RESULT_SUCCESS != result || iafPortState.healthStatus != IAF_FPORT_HEALTH_HEALTHY) {
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

void FabricSubDeviceIaf::setIafNlApi(IafNlApi *iafNlApi) {
    pIafNlApi.reset(iafNlApi);
}

std::unique_ptr<FabricDeviceInterface> FabricDeviceInterface::createFabricDeviceInterface(const FabricVertex &fabricVertex) {

    ze_fabric_vertex_exp_properties_t vertexProperties = {};
    fabricVertex.getProperties(&vertexProperties);

    ze_device_handle_t hDevice = nullptr;
    fabricVertex.getDevice(&hDevice);
    DEBUG_BREAK_IF(hDevice == nullptr);

    DeviceImp *deviceImp = static_cast<DeviceImp *>(hDevice);
    Device *device = static_cast<Device *>(hDevice);
    if (deviceImp->isSubdevice) {
        return std::unique_ptr<FabricDeviceInterface>(new (std::nothrow) FabricSubDeviceIaf(device));
    }

    return std::unique_ptr<FabricDeviceInterface>(new (std::nothrow) FabricDeviceIaf(device));
}

} // namespace L0
