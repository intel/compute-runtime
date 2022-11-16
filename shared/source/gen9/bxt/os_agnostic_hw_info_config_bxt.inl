/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "product_family.h"

namespace NEO {
template <>
AOT::PRODUCT_CONFIG HwInfoConfigHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return AOT::APL;
}

template <>
std::optional<aub_stream::ProductFamily> HwInfoConfigHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Bxt;
};

} // namespace NEO
