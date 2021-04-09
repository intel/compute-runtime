/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "level_zero/core/source/device/device_imp.h"

namespace L0 {

NEO::Device *DeviceImp::getActiveDevice() const {
    if (isMultiDeviceCapable()) {
        return this->neoDevice->getDeviceById(0);
    }
    return this->neoDevice;
}

bool DeviceImp::isMultiDeviceCapable() const {
    return neoDevice->getNumAvailableDevices() > 1u;
}

void DeviceImp::processAdditionalKernelProperties(NEO::HwHelper &hwHelper, ze_device_module_properties_t *pKernelProperties) {
}

ze_result_t DeviceImp::getMemoryPropertiesImp(uint32_t *pCount, ze_device_memory_properties_t *pMemProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
