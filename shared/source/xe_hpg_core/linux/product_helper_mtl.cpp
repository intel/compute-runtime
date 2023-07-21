/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_dg2_and_later.inl"
#include "shared/source/os_interface/product_helper_xehp_and_later.inl"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"

constexpr static auto gfxProduct = IGFX_METEORLAKE;

namespace NEO {
#include "shared/source/os_interface/linux/product_helper_mtl_and_later.inl"
} // namespace NEO

#include "shared/source/xe_hpg_core/mtl/os_agnostic_product_helper_mtl.inl"

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

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
