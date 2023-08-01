/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/cfl/device_ids_configs_cfl.h"
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
uint64_t CompilerProductHelperHw<IGFX_COFFEELAKE>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x100030006;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_COFFEELAKE>::getDefaultHwIpVersion() const {
    return AOT::CFL;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_COFFEELAKE>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    auto deviceId = hwInfo.platform.usDeviceID;
    bool isCfl = (std::find(cflDeviceIds.begin(), cflDeviceIds.end(), deviceId) != cflDeviceIds.end());
    bool isWhl = (std::find(whlDeviceIds.begin(), whlDeviceIds.end(), deviceId) != whlDeviceIds.end());
    bool isCml = (std::find(cmlDeviceIds.begin(), cmlDeviceIds.end(), deviceId) != cmlDeviceIds.end());

    if (isCfl) {
        return AOT::CFL;
    } else if (isCml) {
        return AOT::CML;
    } else if (isWhl) {
        return AOT::WHL;
    }
    return getDefaultHwIpVersion();
}

static EnableCompilerProductHelper<IGFX_COFFEELAKE> enableCompilerProductHelperCFL;

} // namespace NEO
