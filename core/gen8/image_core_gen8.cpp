/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen8/hw_cmds.h"
#include "core/image/image_surface_state.h"

namespace NEO {

typedef BDWFamily Family;

template <>
void setMipTailStartLod<Family>(Family::RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {}

// clang-format off
#include "core/image/image_bdw_plus.inl"
// clang-format on
} // namespace NEO
