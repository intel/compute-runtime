/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {
template <>
uint32_t ProductHelperHw<gfxProduct>::getAvailableSlmSizePerSubslice(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    return hwInfo.gtSystemInfo.SLMSizeInKb;
}

template <>
bool ProductHelperHw<gfxProduct>::isTlbFlushRequired() const {
    bool tlbFlushRequired = false;
    if (debugManager.flags.ForceTlbFlush.get() != -1) {
        tlbFlushRequired = !!debugManager.flags.ForceTlbFlush.get();
    }
    return tlbFlushRequired;
}
} // namespace NEO
