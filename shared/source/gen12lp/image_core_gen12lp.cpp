/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/image/image_surface_state.h"

namespace NEO {

using GfxFamily = Gen12LpFamily;
}

#include "shared/source/image/image_surface_state.inl"
#include "shared/source/image/image_surface_state_before_xe2.inl"

namespace NEO {
template class ImageSurfaceStateHelper<GfxFamily>;
} // namespace NEO
