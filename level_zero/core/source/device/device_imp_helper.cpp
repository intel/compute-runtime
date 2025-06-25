/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device_imp.h"

namespace L0 {

uint32_t DeviceImp::getAdditionalEngines(uint32_t numAdditionalEnginesRequested,
                                         ze_command_queue_group_properties_t *pCommandQueueGroupProperties) {
    return 0;
}

void DeviceImp::getExtendedDeviceModuleProperties(ze_base_desc_t *pExtendedProperties) {}

void DeviceImp::getAdditionalExtProperties(ze_base_properties_t *extendedProperties) {}

void DeviceImp::getAdditionalMemoryExtProperties(ze_base_properties_t *extProperties, const NEO::HardwareInfo &hwInfo) {}

} // namespace L0
