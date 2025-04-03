/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {
template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    hwInfo->featureTable.flags.ftr57bGPUAddressing = (hwInfo->capabilityTable.gpuAddressSpace == maxNBitValue(57));

    enableBlitterOperationsSupport(hwInfo);

    auto &kmdNotifyProperties = hwInfo->capabilityTable.kmdNotifyProperties;
    kmdNotifyProperties.enableKmdNotify = true;
    kmdNotifyProperties.delayKmdNotifyMicroseconds = 150;
    kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission = true;
    kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds = 20;

    return 0;
}

template <>
bool ProductHelperHw<gfxProduct>::isNonBlockingGpuSubmissionSupported() const {
    return false;
}

} // namespace NEO
