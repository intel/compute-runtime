/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aubstream/product_family.h"

namespace NEO {
template <>
AOT::PRODUCT_CONFIG ProductHelperHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return AOT::SKL;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Skl;
};

template <>
uint32_t ProductHelperHw<gfxProduct>::getDefaultRevisionId() const {
    return 9u;
}

} // namespace NEO
