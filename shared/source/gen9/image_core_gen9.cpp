/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/image/image_surface_state.h"

namespace NEO {

typedef SKLFamily Family;

// clang-format off
#include "shared/source/image/image_bdw_and_later.inl"
#include "shared/source/image/image_skl_and_later.inl"
// clang-format on
} // namespace NEO
