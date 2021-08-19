/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

namespace NEO {
constexpr static auto gfxProduct = IGFX_ALDERLAKE_P;

#include "shared/source/gen12lp/os_agnostic_hw_info_config_gen12lp.inl"

template <>
void HwInfoConfigHw<IGFX_ALDERLAKE_P>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    PLATFORM *platform = &hwInfo->platform;
    platform->eRenderCoreFamily = IGFX_GEN12LP_CORE;
    platform->eDisplayCoreFamily = IGFX_GEN12LP_CORE;
}

template class HwInfoConfigHw<IGFX_ALDERLAKE_P>;

} // namespace NEO
