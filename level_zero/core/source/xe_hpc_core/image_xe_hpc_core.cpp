/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds.h"
#include "shared/source/xe_hpc_core/hw_info.h"

#include "level_zero/core/source/image/image_hw.inl"

namespace L0 {

static constexpr auto gfxCoreFamily = IGFX_XE_HPC_CORE;

template struct ImageCoreFamily<gfxCoreFamily>;
static ImagePopulateFactory<gfxCoreFamily, ImageCoreFamily<gfxCoreFamily>> populateXeHpcCore;

} // namespace L0
