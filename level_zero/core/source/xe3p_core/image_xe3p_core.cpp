/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/image/image_hw.inl"

#include "hw_cmds_xe3p_core.h"

namespace L0 {

static constexpr auto gfxCoreFamily = IGFX_XE3P_CORE;

template struct ImageCoreFamily<gfxCoreFamily>;
static ImagePopulateFactory<gfxCoreFamily, ImageCoreFamily<gfxCoreFamily>> populateXe3pCore;

} // namespace L0
