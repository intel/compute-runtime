/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "product_family.h"

namespace NEO {
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

template <>
std::optional<aub_stream::ProductFamily> HwInfoConfigHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Icllp;
};

} // namespace NEO
