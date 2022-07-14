/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"

#include "level_zero/core/source/helpers/l0_populate_factory.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper_base.inl"
#include "level_zero/core/source/hw_helpers/l0_hw_helper_skl_and_later.inl"
#include "level_zero/tools/source/debug/eu_thread.h"

namespace L0 {

using Family = NEO::TGLLPFamily;
static auto gfxCore = IGFX_GEN12LP_CORE;

template <>
void populateFactoryTable<L0HwHelperHw<Family>>() {
    extern L0HwHelper *l0HwHelperFactory[IGFX_MAX_CORE];
    l0HwHelperFactory[gfxCore] = &L0HwHelperHw<Family>::get();
}

template <>
bool L0HwHelperHw<Family>::isResumeWARequired() {
    return true;
}

// clang-format off
#include "level_zero/core/source/hw_helpers/l0_hw_helper_tgllp_plus.inl"
// clang-format on

template class L0HwHelperHw<Family>;

} // namespace L0
