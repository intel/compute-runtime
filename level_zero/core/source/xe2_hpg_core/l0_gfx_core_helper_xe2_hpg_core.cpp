/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_base.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_pvc_and_later.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_pvc_to_xe2.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xe2_hpg_and_later.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xe_hpg_to_xe2_hpg.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xehp_and_later.inl"
#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace L0 {
using Family = NEO::Xe2HpgCoreFamily;
static auto gfxCore = IGFX_XE2_HPG_CORE;

template <>
zet_debug_regset_type_intel_gpu_t L0GfxCoreHelperHw<Family>::getRegsetTypeForLargeGrfDetection() const {
    return ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU;
}

template <typename Family>
uint32_t L0GfxCoreHelperHw<Family>::getGrfRegisterCount(uint32_t *regPtr) const {
    bool largeGrfModeEnabled = false;
    largeGrfModeEnabled = ((regPtr[1] & 0x6000) == 0x6000);
    if (largeGrfModeEnabled) {
        return 256;
    }
    return 128;
}

template <>
bool L0GfxCoreHelperHw<Family>::alwaysAllocateEventInLocalMem() const {
    return true;
}

#include "level_zero/core/source/helpers/l0_gfx_core_helper_factory_init.inl"

template class L0GfxCoreHelperHw<Family>;
} // namespace L0
