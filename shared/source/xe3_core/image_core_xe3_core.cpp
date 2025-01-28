/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/image/image_surface_state.h"
#include "shared/source/xe3_core/hw_cmds_base.h"

namespace NEO {

using GfxFamily = Xe3CoreFamily;
}
#include "shared/source/image/image_surface_state.inl"
namespace NEO {
template class ImageSurfaceStateHelper<GfxFamily>;
} // namespace NEO
