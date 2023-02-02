/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_base.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_tgllp_to_dg2.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xehp_and_later.inl"
#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace L0 {

using Family = NEO::XeHpFamily;
static auto gfxCore = IGFX_XE_HP_CORE;

template <>
void populateFactoryTable<L0GfxCoreHelperHw<Family>>() {
    extern L0GfxCoreHelper *l0GfxCoreHelperFactory[IGFX_MAX_CORE];
    l0GfxCoreHelperFactory[gfxCore] = &L0GfxCoreHelperHw<Family>::get();
}

template <>
bool L0GfxCoreHelperHw<Family>::isResumeWARequired() {
    return true;
}

template <>
bool L0GfxCoreHelperHw<Family>::multiTileCapablePlatform() const {
    return true;
}

template <>
void L0GfxCoreHelperHw<Family>::setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, NEO::EngineGroupT &group) const {
}

template <>
bool L0GfxCoreHelperHw<Family>::platformSupportsRayTracing() const {
    return true;
}

template class L0GfxCoreHelperHw<Family>;

} // namespace L0
