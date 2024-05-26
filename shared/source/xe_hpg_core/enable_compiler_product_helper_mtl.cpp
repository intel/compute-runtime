/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"

constexpr auto gfxProduct = IGFX_METEORLAKE;

#include "shared/source/xe_hpg_core/xe_lpg/compiler_product_helper_xe_lpg.inl"

namespace NEO {
template <>
uint32_t CompilerProductHelperHw<gfxProduct>::getDefaultHwIpVersion() const {
    return AOT::MTL_M_B0;
}

template <>
uint32_t CompilerProductHelperHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    if (hwInfo.ipVersion.value) {
        return hwInfo.ipVersion.value;
    }
    switch (hwInfo.platform.usDeviceID) {
    case 0x7D40:
    case 0x7D45: {
        switch (hwInfo.platform.usRevId) {
        case 0x0:
        case 0x2:
            return AOT::MTL_M_A0;
        case 0x3:
        case 0x8:
            return AOT::MTL_M_B0;
        }
        break;
    }
    case 0x7D55:
    case 0X7DD5: {
        switch (hwInfo.platform.usRevId) {
        case 0x0:
        case 0x2:
            return AOT::MTL_P_A0;
        case 0x3:
        case 0x8:
            return AOT::MTL_P_B0;
        }
        break;
    }
    case 0x7D60: {
        switch (hwInfo.platform.usRevId) {
        case 0x0:
            return AOT::MTL_M_A0;
        case 0x2:
            return AOT::MTL_M_B0;
        }
        break;
    }
    }
    return getDefaultHwIpVersion();
}

} // namespace NEO

static NEO::EnableCompilerProductHelper<gfxProduct> enableCompilerProductHelperMTL;
