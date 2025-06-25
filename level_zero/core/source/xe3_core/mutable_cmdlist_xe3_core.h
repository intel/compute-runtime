/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/xe3_core/hw_cmds_base.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.h"

namespace L0::MCL {
template <PRODUCT_FAMILY productFamily>
struct MutableCommandListProductFamily : public MutableCommandListCoreFamily<IGFX_XE3_CORE> {
    using MutableCommandListCoreFamily::MutableCommandListCoreFamily;
};
} // namespace L0::MCL
