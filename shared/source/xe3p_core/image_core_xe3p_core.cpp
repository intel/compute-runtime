/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/image/image_surface_state.h"
#include "shared/source/xe3p_core/hw_cmds_base.h"

namespace NEO {

using GfxFamily = Xe3pCoreFamily;

}

#include "shared/source/image/image_surface_state.inl"
#include "shared/source/image/image_surface_state_xe2_and_later.inl"

namespace NEO {
template class ImageSurfaceStateHelper<GfxFamily>;
} // namespace NEO
