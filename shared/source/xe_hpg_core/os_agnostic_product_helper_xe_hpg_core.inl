/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_before_xe2.inl"
#include "shared/source/os_interface/product_helper_from_xe_hpg_to_xe3.inl"
#include "shared/source/os_interface/product_helper_xe_hpg_and_later.inl"

namespace NEO {

template <>
uint32_t ProductHelperHw<gfxProduct>::getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const {
    if (isMaxThreadsForWorkgroupWARequired(hwInfo)) {
        return std::min(getMaxThreadsForWorkgroup(hwInfo, maxNumEUsPerDualSubSlice), 64u);
    }
    return getMaxThreadsForWorkgroup(hwInfo, maxNumEUsPerDualSubSlice);
}

} // namespace NEO
