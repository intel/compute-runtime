/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/image/image_surface_state.h"
#include "shared/source/xe_hp_core/hw_cmds_base.h"

namespace NEO {

using Family = XeHpFamily;

// clang-format off
#include "shared/source/image/image_bdw_and_later.inl"
#include "shared/source/image/image_skl_and_later.inl"
// clang-format on
} // namespace NEO
