/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/image/image_surface_state.h"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

namespace NEO {

using Family = Xe2HpgCoreFamily;

#include "shared/source/image/image_skl_and_later.inl"
} // namespace NEO
