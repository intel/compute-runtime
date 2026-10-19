/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"

namespace NEO {
template <>
uint32_t ProductHelperHw<gfxProduct>::getAvailableSlmSizePerSubslice(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    return hwInfo.gtSystemInfo.SLMSizeInKb;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::overrideDirectSubmissionTimeouts(uint64_t &timeoutUs, uint64_t &maxTimeoutUs) const {
    timeoutUs = 1'000;
    maxTimeoutUs = 1'000;
    if (debugManager.flags.DirectSubmissionControllerTimeout.get() != -1) {
        timeoutUs = debugManager.flags.DirectSubmissionControllerTimeout.get();
    }
    if (debugManager.flags.DirectSubmissionControllerMaxTimeout.get() != -1) {
        maxTimeoutUs = debugManager.flags.DirectSubmissionControllerMaxTimeout.get();
    }
}
} // namespace NEO
