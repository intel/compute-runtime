/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"

#include "level_zero/core/source/helpers/l0_populate_factory.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper_base.inl"
#include "level_zero/core/source/hw_helpers/l0_hw_helper_skl_to_tgllp.inl"
#include "level_zero/core/source/hw_helpers/l0_hw_helper_tgllp_to_dg2.inl"
#include "level_zero/tools/source/debug/eu_thread.h"

namespace L0 {

using Family = NEO::Gen12LpFamily;
static auto gfxCore = IGFX_GEN12LP_CORE;

#include "level_zero/core/source/helpers/l0_hw_helper_factory_init.inl"

template <>
bool L0GfxCoreHelperHw<Family>::isResumeWARequired() {
    return true;
}

template class L0GfxCoreHelperHw<Family>;

} // namespace L0
