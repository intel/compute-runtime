/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_arl.h"

constexpr auto gfxProduct = IGFX_ARROWLAKE;

#include "shared/source/xe_hpg_core/xe_lpg/compiler_product_helper_xe_lpg.inl"

#include "additional_product_config_arl.inl"

namespace NEO {
template <>
uint32_t CompilerProductHelperHw<gfxProduct>::getDefaultHwIpVersion() const {
    return AOT::ARL_H_B0;
}

template <>
uint32_t CompilerProductHelperHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    if (hwInfo.ipVersion.value) {
        return hwInfo.ipVersion.value;
    }
    switch (hwInfo.platform.usDeviceID) {
    case 0x7D67:
    case 0x7D41: {
        switch (hwInfo.platform.usRevId) {
        case 0x0:
            return AOT::MTL_U_A0;
        case 0x3:
        case 0x6:
            return AOT::MTL_U_B0;
        }
        break;
    }
    case 0x7D51:
    case 0X7DD1: {
        switch (hwInfo.platform.usRevId) {
        case 0x0:
        case 0x3:
            return AOT::ARL_H_A0;
        case 0x6:
            return AOT::ARL_H_B0;
        }
        break;
    }
    }
    return getProductConfigFromHwInfoAdditionalArl(*this, hwInfo);
}

} // namespace NEO
static NEO::EnableCompilerProductHelper<gfxProduct> enableCompilerProductHelperARL;
