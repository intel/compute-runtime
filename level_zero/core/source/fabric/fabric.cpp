/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/fabric/fabric.h"

#include "shared/source/helpers/string.h"

#include "level_zero/core/source/device/device_imp.h"

namespace L0 {

FabricVertex *FabricVertex::createFromDevice(Device *device) {

    auto fabricVertex = new FabricVertex();
    UNRECOVERABLE_IF(fabricVertex == nullptr);
    ze_device_properties_t deviceProperties = {};
    ze_pci_ext_properties_t pciProperties = {};

    deviceProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    pciProperties.stype = ZE_STRUCTURE_TYPE_PCI_EXT_PROPERTIES;

    device->getProperties(&deviceProperties);

    fabricVertex->properties.stype = ZE_STRUCTURE_TYPE_FABRIC_VERTEX_EXP_PROPERTIES;
    memcpy_s(fabricVertex->properties.uuid.id, ZE_MAX_UUID_SIZE, deviceProperties.uuid.id, ZE_MAX_DEVICE_UUID_SIZE);

    if (deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) {
        fabricVertex->properties.type = ZE_FABRIC_VERTEX_EXP_TYPE_SUBEVICE;
    } else {
        fabricVertex->properties.type = ZE_FABRIC_VERTEX_EXP_TYPE_DEVICE;
    }
    fabricVertex->properties.remote = false;
    fabricVertex->device = device;
    if (device->getPciProperties(&pciProperties) == ZE_RESULT_SUCCESS) {
        fabricVertex->properties.address.domain = pciProperties.address.domain;
        fabricVertex->properties.address.bus = pciProperties.address.bus;
        fabricVertex->properties.address.device = pciProperties.address.device;
        fabricVertex->properties.address.function = pciProperties.address.function;
    }

    return fabricVertex;
}

ze_result_t FabricVertex::getSubVertices(uint32_t *pCount, ze_fabric_vertex_handle_t *phSubvertices) {

    auto deviceImp = static_cast<DeviceImp *>(device);
    if (*pCount == 0) {
        *pCount = deviceImp->numSubDevices;
        return ZE_RESULT_SUCCESS;
    }

    *pCount = std::min(deviceImp->numSubDevices, *pCount);
    for (uint32_t index = 0; index < *pCount; index++) {
        auto subDeviceImp = static_cast<DeviceImp *>(deviceImp->subDevices[index]);
        phSubvertices[index] = subDeviceImp->fabricVertex->toHandle();
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricVertex::getProperties(ze_fabric_vertex_exp_properties_t *pVertexProperties) {
    *pVertexProperties = properties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricVertex::getDevice(ze_device_handle_t *phDevice) {

    *phDevice = device->toHandle();
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
