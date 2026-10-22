/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/windows/sharedata_wrapper.h"

#include <array>
#include <string>
#include <utility>

namespace NEO {

inline constexpr std::array<std::pair<PRODUCT_FAMILY, GFXCORE_FAMILY>, 8> renderCoreFamilyMap = {{
    {IGFX_TIGERLAKE_LP, IGFX_GEN12LP_CORE},
    {IGFX_ROCKETLAKE, IGFX_GEN12LP_CORE},
    {IGFX_ALDERLAKE_S, IGFX_GEN12LP_CORE},
    {IGFX_ALDERLAKE_P, IGFX_GEN12LP_CORE},
    {IGFX_ALDERLAKE_N, IGFX_GEN12LP_CORE},
    {IGFX_DG1, IGFX_GEN12LP_CORE},
    {IGFX_NVL, IGFX_XE3P_CORE},
    {IGFX_CRI, IGFX_XE3P_CORE},
}};

template <typename CoreFamilyMapT>
bool mapRenderCoreFamily(PLATFORM &platform, const CoreFamilyMapT &coreFamilyMap) {
    for (const auto &[productFamily, coreFamily] : coreFamilyMap) {
        if (platform.eProductFamily == productFamily) {
            platform.eRenderCoreFamily = coreFamily;
            return true;
        }
    }
    return false;
}

inline void applyPlatformDebugOverrides(PLATFORM &platform) {
    if (debugManager.flags.ForceDeviceId.get() != "unk") {
        platform.usDeviceID = static_cast<unsigned short>(std::stoi(debugManager.flags.ForceDeviceId.get(), nullptr, 16));
    }
    if (debugManager.flags.ForceProductFamily.get() != "unk") {
        platform.eProductFamily = static_cast<PRODUCT_FAMILY>(std::stoi(debugManager.flags.ForceProductFamily.get(), nullptr, 0));
    }
    if (debugManager.flags.ForceCoreFamily.get() != "unk") {
        platform.eRenderCoreFamily = static_cast<GFXCORE_FAMILY>(std::stoi(debugManager.flags.ForceCoreFamily.get(), nullptr, 0));
    }
}

} // namespace NEO
