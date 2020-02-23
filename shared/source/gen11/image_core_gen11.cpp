/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds_base.h"
#include "shared/source/image/image_surface_state.h"

namespace NEO {

typedef ICLFamily Family;

// clang-format off
#include "shared/source/image/image_bdw_plus.inl"
#include "shared/source/image/image_skl_plus.inl"
// clang-format on
} // namespace NEO
