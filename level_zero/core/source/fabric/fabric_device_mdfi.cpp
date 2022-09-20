/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/fabric/fabric.h"
#include "level_zero/core/source/fabric/fabric_device_interface.h"

namespace L0 {

bool FabricDeviceMdfi::getEdgeProperty(FabricVertex *neighborVertex, ze_fabric_edge_exp_properties_t &edgeProperty) {

    DeviceImp *currentDeviceImp = static_cast<DeviceImp *>(device);
    DeviceImp *neighborDeviceImp = static_cast<DeviceImp *>(neighborVertex->device);

    // Ignore root devices
    if (!currentDeviceImp->isSubdevice || !neighborDeviceImp->isSubdevice) {
        return false;
    }

    const uint32_t currRootDeviceIndex = device->getRootDeviceIndex();
    const uint32_t neighborRootDeviceIndex = neighborVertex->device->getRootDeviceIndex();
    const uint32_t currSubDeviceId = static_cast<DeviceImp *>(device)->getPhysicalSubDeviceId();
    const uint32_t neighborSubDeviceId = static_cast<DeviceImp *>(neighborVertex->device)->getPhysicalSubDeviceId();

    if (currRootDeviceIndex == neighborRootDeviceIndex &&
        currSubDeviceId < neighborSubDeviceId) {

        ze_uuid_t &uuid = edgeProperty.uuid;

        std::memset(uuid.id, 0, ZE_MAX_UUID_SIZE);
        // Copy current Root DeviceIndex and SubDeviceId
        memcpy_s(&uuid.id[0], 4, &currRootDeviceIndex, 4);
        memcpy_s(&uuid.id[4], 1, &currSubDeviceId, 1);
        // Copy neighboring Root DeviceIndex and SubDeviceId
        memcpy_s(&uuid.id[8], 4, &neighborRootDeviceIndex, 4);
        memcpy_s(&uuid.id[12], 1, &neighborSubDeviceId, 1);

        memcpy_s(edgeProperty.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, "MDFI", 5);
        edgeProperty.bandwidth = 0;
        edgeProperty.bandwidthUnit = ZE_BANDWIDTH_UNIT_UNKNOWN;
        edgeProperty.latency = 0;
        edgeProperty.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
        edgeProperty.duplexity = ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX;
        return true;
    }
    return false;
}

std::unique_ptr<FabricDeviceInterface> FabricDeviceInterface::createFabricDeviceInterfaceMdfi(const FabricVertex *fabricVertex) {

    return std::unique_ptr<FabricDeviceInterface>(new (std::nothrow) FabricDeviceMdfi(fabricVertex->device));
}

} // namespace L0
