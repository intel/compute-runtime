/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/image/image_surface_state.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

namespace NEO {

using GfxFamily = XeHpcCoreFamily;
}
#include "shared/source/image/image_surface_state.inl"
#include "shared/source/image/image_surface_state_before_xe2.inl"
namespace NEO {
template class ImageSurfaceStateHelper<GfxFamily>;
} // namespace NEO
