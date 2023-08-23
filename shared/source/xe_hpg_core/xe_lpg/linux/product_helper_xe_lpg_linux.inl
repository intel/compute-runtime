/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/product_helper_mtl_and_later.inl"
#include "shared/source/os_interface/product_helper_hw.h"
namespace NEO {
template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    enableCompression(hwInfo);

    enableBlitterOperationsSupport(hwInfo);

    auto &kmdNotifyProperties = hwInfo->capabilityTable.kmdNotifyProperties;
    kmdNotifyProperties.enableKmdNotify = true;
    kmdNotifyProperties.delayKmdNotifyMicroseconds = 150;
    kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission = true;
    kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds = 20;

    return 0;
}

template <>
bool ProductHelperHw<gfxProduct>::isBlitterForImagesSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isVmBindPatIndexProgrammingSupported() const {
    return true;
}
} // namespace NEO
