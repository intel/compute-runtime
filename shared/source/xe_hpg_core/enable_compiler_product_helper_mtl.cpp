/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/compiler_product_helper_base.inl"
#include "shared/source/helpers/compiler_product_helper_bdw_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_before_xe_hpc.inl"
#include "shared/source/helpers/compiler_product_helper_enable_subgroup_local_block_io.inl"
#include "shared/source/helpers/compiler_product_helper_mtl_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_tgllp_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_xe_hp_and_later.inl"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"

#include "compiler_product_helper_mtl.inl"
#include "platforms.h"

constexpr auto gfxProduct = IGFX_METEORLAKE;

namespace NEO {
template <>
uint32_t CompilerProductHelperHw<gfxProduct>::getDefaultHwIpVersion() const {
    return AOT::MTL_M_A0;
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

static EnableCompilerProductHelper<gfxProduct> enableCompilerProductHelperMTL;

} // namespace NEO
