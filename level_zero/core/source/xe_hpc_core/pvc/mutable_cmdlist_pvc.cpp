/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.inl"
#include "level_zero/core/source/xe_hpc_core/mutable_cmdlist_xe_hpc_core.h"

namespace L0::MCL {
static MutableCommandListPopulateFactory<IGFX_PVC, MutableCommandListProductFamily<IGFX_PVC>>
    populateMutablePVC;
} // namespace L0::MCL
