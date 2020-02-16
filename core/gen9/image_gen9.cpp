/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen9/hw_cmds.h"
#include "core/image/image_surface_state.h"

namespace NEO {

typedef SKLFamily Family;

// clang-format off
#include "core/image/image_bdw_plus.inl"
#include "core/image/image_skl_plus.inl"
// clang-format on
} // namespace NEO
