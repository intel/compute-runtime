/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aubstream/product_family.h"

namespace NEO {
template <>
uint32_t ProductHelperHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    switch (stepping) {
    case REVISION_A0:
        return 0x0;
    case REVISION_B:
        return 0x4;
    }
    return CommonConstants::invalidStepping;
}

template <>
AOT::PRODUCT_CONFIG ProductHelperHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return AOT::ADL_S;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (hwInfo.platform.usRevId) {
    case 0x0:
        return REVISION_A0;
    case 0x4:
        return REVISION_B;
    }
    return CommonConstants::invalidStepping;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Adls;
};

template <>
bool ProductHelperHw<gfxProduct>::pipeControlWARequired(const HardwareInfo &hwInfo) const {
    return GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, *this);
}

} // namespace NEO
