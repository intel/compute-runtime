/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <>
AOT::PRODUCT_CONFIG ProductHelperHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return AOT::EHL;
}
} // namespace NEO
