/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "shared/source/image/image_surface_state.h"

namespace NEO {

typedef Gen8Family Family;

template <>
void setMipTailStartLod<Family>(Family::RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {}

// clang-format off
#include "shared/source/image/image_bdw_and_later.inl"
// clang-format on
} // namespace NEO
