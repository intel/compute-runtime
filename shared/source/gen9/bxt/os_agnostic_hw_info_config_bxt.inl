/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aubstream/product_family.h"

namespace NEO {
template <>
AOT::PRODUCT_CONFIG ProductHelperHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return AOT::APL;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Bxt;
};

} // namespace NEO
