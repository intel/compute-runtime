/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen12lp/hw_cmds.h"
#include "core/image/image_surface_state.h"

namespace NEO {

using Family = TGLLPFamily;

// clang-format off
#include "core/image/image_tgllp_plus.inl"
#include "core/image/image_skl_plus.inl"
// clang-format on
} // namespace NEO
