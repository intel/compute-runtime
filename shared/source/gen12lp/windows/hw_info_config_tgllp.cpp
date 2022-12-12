/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_tgllp.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

#include "platforms.h"

constexpr static auto gfxProduct = IGFX_TIGERLAKE_LP;

#include "shared/source/gen12lp/os_agnostic_hw_info_config_gen12lp.inl"
#include "shared/source/gen12lp/tgllp/os_agnostic_hw_info_config_tgllp.inl"

namespace NEO {

template <>
void ProductHelperHw<gfxProduct>::setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) {
    coherencyFlag = true;
    GfxCoreHelper &gfxCoreHelper = GfxCoreHelper::get(hwInfo.platform.eRenderCoreFamily);
    if (gfxCoreHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo)) {
        // stepping A devices - turn off coherency
        coherencyFlag = false;
    }
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
