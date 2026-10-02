/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_base.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_pvc_and_later.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_pvc_to_xe3p.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xe2_hpg_and_later.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xe3_and_later.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xe3p_and_later.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xehp_and_later.inl"
#include "level_zero/core/source/helpers/l0_populate_factory.h"

#include "hw_cmds_xe3p_core.h"

namespace L0 {
using Family = NEO::Xe3pCoreFamily;
static auto gfxCore = IGFX_XE3P_CORE;

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xe3p_and_later_specializations.inl"
#include "level_zero/core/source/helpers/l0_gfx_core_helper_factory_init.inl"

template <>
NEO::HeapAddressModel L0GfxCoreHelperHw<Family>::getPlatformHeapAddressModel(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (hwInfo.capabilityTable.supportsImages) {
        return NEO::HeapAddressModel::privateHeaps;
    } else {
        return NEO::HeapAddressModel::globalStateless;
    }
}

template class L0GfxCoreHelperHw<Family>;
} // namespace L0
