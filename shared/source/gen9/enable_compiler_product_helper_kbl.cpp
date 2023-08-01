/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/kbl/device_ids_configs_kbl.h"
#include "shared/source/helpers/compiler_aot_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/compiler_product_helper_base.inl"
#include "shared/source/helpers/compiler_product_helper_bdw_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_bdw_to_icllp.inl"
#include "shared/source/helpers/compiler_product_helper_before_xe_hp.inl"
#include "shared/source/helpers/compiler_product_helper_before_xe_hpc.inl"
#include "shared/source/helpers/compiler_product_helper_disable_subgroup_local_block_io.inl"

#include "platforms.h"

#include <algorithm>

namespace NEO {
template <>
uint64_t CompilerProductHelperHw<IGFX_KABYLAKE>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x100030006;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_KABYLAKE>::getDefaultHwIpVersion() const {
    return AOT::KBL;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_KABYLAKE>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    auto deviceId = hwInfo.platform.usDeviceID;
    bool isKbl = (std::find(kblDeviceIds.begin(), kblDeviceIds.end(), deviceId) != kblDeviceIds.end());
    bool isAml = (std::find(amlDeviceIds.begin(), amlDeviceIds.end(), deviceId) != amlDeviceIds.end());

    if (isKbl) {
        return AOT::KBL;
    } else if (isAml) {
        return AOT::AML;
    }
    return getDefaultHwIpVersion();
}

static EnableCompilerProductHelper<IGFX_KABYLAKE> enableCompilerProductHelperKBL;

} // namespace NEO
