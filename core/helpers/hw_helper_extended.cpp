/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/debug_settings/debug_settings_manager.h"
#include "core/helpers/hw_helper.h"

namespace NEO {

uint32_t HwHelper::getSubDevicesCount(const HardwareInfo *pHwInfo) {
    return DebugManager.flags.CreateMultipleSubDevices.get() > 0 ? DebugManager.flags.CreateMultipleSubDevices.get() : 1u;
}

uint32_t HwHelper::getEnginesCount(const HardwareInfo &hwInfo) {
    return 1u;
}

} // namespace NEO
