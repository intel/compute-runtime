/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_aot_config_tgllp_and_later.inl"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/compiler_product_helper_base.inl"
#include "shared/source/helpers/compiler_product_helper_before_xe_hpc.inl"
#include "shared/source/helpers/compiler_product_helper_enable_subgroup_local_block_io.inl"
#include "shared/source/helpers/compiler_product_helper_tgllp_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_xe_hp_and_later.inl"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"

#include "neo_aot_platforms.h"

namespace NEO {
template <>
uint64_t CompilerProductHelperHw<IGFX_DG2>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    if (DG2::isG10(hwInfo)) {
        return 0x800040010;
    }
    return 0x200040010;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_DG2>::getDefaultHwIpVersion() const {
    return AOT::DG2_G10_C0;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_DG2>::matchRevisionIdWithProductConfig(HardwareIpVersion ipVersion, uint32_t revisionID) const {
    HardwareIpVersion dg2Config = ipVersion;
    dg2Config.revision = revisionID;
    return dg2Config.value;
}
template <>
uint32_t CompilerProductHelperHw<IGFX_DG2>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    if (DG2::isG10(hwInfo)) {
        switch (hwInfo.platform.usRevId) {
        case 0x0:
            return AOT::DG2_G10_A0;
        case 0x1:
            return AOT::DG2_G10_A1;
        case 0x4:
            return AOT::DG2_G10_B0;
        case 0x8:
            return AOT::DG2_G10_C0;
        }
    } else if (DG2::isG11(hwInfo)) {
        switch (hwInfo.platform.usRevId) {
        case 0x0:
            return AOT::DG2_G11_A0;
        case 0x4:
            return AOT::DG2_G11_B0;
        case 0x5:
            return AOT::DG2_G11_B1;
        }
    } else if (DG2::isG12(hwInfo)) {
        return AOT::DG2_G12_A0;
    }
    return getDefaultHwIpVersion();
}

static EnableCompilerProductHelper<IGFX_DG2> enableCompilerProductHelperDG2;
} // namespace NEO
