/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"

namespace L0 {

uint32_t Device::getAdditionalEngines(uint32_t numAdditionalEnginesRequested,
                                      ze_command_queue_group_properties_t *pCommandQueueGroupProperties) {
    return 0;
}

void Device::getExtendedDeviceModuleProperties(ze_base_desc_t *pExtendedProperties) {}

void Device::getAdditionalExtProperties(ze_base_properties_t *extendedProperties) {}

} // namespace L0
