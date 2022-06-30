/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
bool HwInfoConfigHw<gfxProduct>::isAdditionalMediaSamplerProgrammingRequired() const {
    return true;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isInitialFlagsProgrammingRequired() const {
    return true;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isReturnedCmdSizeForMediaSamplerAdjustmentRequired() const {
    return true;
}

template <>
AOT::PRODUCT_CONFIG HwInfoConfigHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return AOT::ICL;
}