/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.h"

namespace L0::MCL {
template <PRODUCT_FAMILY productFamily>
struct MutableCommandListProductFamily : public MutableCommandListCoreFamily<IGFX_XE2_HPG_CORE> {
    using MutableCommandListCoreFamily::MutableCommandListCoreFamily;
};
} // namespace L0::MCL
