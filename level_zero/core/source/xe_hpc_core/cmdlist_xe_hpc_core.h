/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"

namespace L0 {
template <PRODUCT_FAMILY productFamily>
struct CommandListProductFamily : public CommandListCoreFamily<IGFX_XE_HPC_CORE> {
    using CommandListCoreFamily::CommandListCoreFamily;
};

template <PRODUCT_FAMILY gfxProductFamily>
struct CommandListImmediateProductFamily : public CommandListCoreFamilyImmediate<IGFX_XE_HPC_CORE> {
    using CommandListCoreFamilyImmediate::CommandListCoreFamilyImmediate;
};
} // namespace L0
