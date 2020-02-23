/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/image/image_surface_state.h"

namespace NEO {

using Family = TGLLPFamily;

// clang-format off
#include "shared/source/image/image_tgllp_plus.inl"
#include "shared/source/image/image_skl_plus.inl"
// clang-format on
} // namespace NEO
