/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/image/image_surface_state.h"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

namespace NEO {

using GfxFamily = Xe2HpgCoreFamily;
}

#include "shared/source/image/image_surface_state.inl"
#include "shared/source/image/image_surface_state_xe2_and_later.inl"

namespace NEO {
template class ImageSurfaceStateHelper<GfxFamily>;
} // namespace NEO
