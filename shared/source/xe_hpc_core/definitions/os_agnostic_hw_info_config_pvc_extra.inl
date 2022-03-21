/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
bool HwInfoConfigHw<gfxProduct>::isComputeDispatchAllWalkerEnableInComputeWalkerRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const {
    auto revId = hwInfo.platform.usRevId & XE_HPC_COREFamily::pvcSteppingBits;
    return (0x3 <= revId) ? 16u : 8u;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isComputeDispatchAllWalkerEnableInCfeStateRequired(const HardwareInfo &hwInfo) const {
    return true;
}