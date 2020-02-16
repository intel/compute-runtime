/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen11/hw_cmds_base.h"
#include "core/image/image_surface_state.h"

namespace NEO {

typedef ICLFamily Family;

// clang-format off
#include "core/image/image_bdw_plus.inl"
#include "core/image/image_skl_plus.inl"
// clang-format on
} // namespace NEO
