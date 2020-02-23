/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gen11/hw_cmds_base.h"
#include "image/image_surface_state.h"

namespace NEO {

typedef ICLFamily Family;

// clang-format off
#include "image/image_bdw_plus.inl"
#include "image/image_skl_plus.inl"
// clang-format on
} // namespace NEO
