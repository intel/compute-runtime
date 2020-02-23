/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gen12lp/hw_cmds.h"
#include "image/image_surface_state.h"

namespace NEO {

using Family = TGLLPFamily;

// clang-format off
#include "image/image_tgllp_plus.inl"
#include "image/image_skl_plus.inl"
// clang-format on
} // namespace NEO
