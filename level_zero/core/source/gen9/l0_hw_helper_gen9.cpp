/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"

#include "level_zero/core/source/helpers/l0_populate_factory.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper_base.inl"
#include "level_zero/core/source/hw_helpers/l0_hw_helper_skl_to_icllp.inl"
#include "level_zero/core/source/hw_helpers/l0_hw_helper_skl_to_tgllp.inl"

namespace L0 {

using Family = NEO::Gen9Family;
static auto gfxCore = IGFX_GEN9_CORE;

template <>
void populateFactoryTable<L0GfxCoreHelperHw<Family>>() {
    extern L0GfxCoreHelper *l0GfxCoreHelperFactory[IGFX_MAX_CORE];
    l0GfxCoreHelperFactory[gfxCore] = &L0GfxCoreHelperHw<Family>::get();
}

template class L0GfxCoreHelperHw<Family>;

} // namespace L0
