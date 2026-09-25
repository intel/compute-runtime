/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3_core/hw_cmds_base.h"
#include "shared/source/xe3_core/hw_info_xe3_core.h"

#include "level_zero/core/source/image/image_hw.inl"

namespace L0 {

static constexpr auto gfxCoreFamily = IGFX_XE3_CORE;

template struct ImageCoreFamily<gfxCoreFamily>;
static ImagePopulateFactory<gfxCoreFamily, ImageCoreFamily<gfxCoreFamily>> populateXe3Core;

} // namespace L0
