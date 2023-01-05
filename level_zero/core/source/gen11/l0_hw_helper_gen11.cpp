/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"

#include "level_zero/core/source/helpers/l0_populate_factory.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper_base.inl"
#include "level_zero/core/source/hw_helpers/l0_hw_helper_skl_to_icllp.inl"
#include "level_zero/core/source/hw_helpers/l0_hw_helper_skl_to_tgllp.inl"

namespace L0 {

using Family = NEO::Gen11Family;
static auto gfxCore = IGFX_GEN11_CORE;

#include "level_zero/core/source/helpers/l0_hw_helper_factory_init.inl"

template class L0GfxCoreHelperHw<Family>;

} // namespace L0
