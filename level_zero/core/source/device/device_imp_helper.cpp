/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"

#include <cstring>

namespace L0 {

bool Device::fabricEdgeModelSupportsBandwidthAndLatency(const char *model) {
    return strstr(model, "XeLink") != nullptr;
}

uint32_t Device::getAdditionalEngines(uint32_t numAdditionalEnginesRequested,
                                      ze_command_queue_group_properties_t *pCommandQueueGroupProperties) {
    return 0;
}

void Device::getAdditionalExtProperties(ze_base_properties_t *extendedProperties) {}

} // namespace L0
