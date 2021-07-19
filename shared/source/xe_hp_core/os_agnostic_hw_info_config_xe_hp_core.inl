/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

using namespace NEO;

template <>
bool HwInfoConfigHw<IGFX_XE_HP_SDV>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    uint32_t stepping = hwHelper.getSteppingFromHwRevId(hwInfo);
    return REVISION_B > stepping;
}