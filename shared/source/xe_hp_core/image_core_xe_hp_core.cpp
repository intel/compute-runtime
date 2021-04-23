/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/image/image_surface_state.h"
#include "shared/source/xe_hp_core/hw_cmds_base.h"

namespace NEO {

using Family = XeHpFamily;

template <>
void setFilterMode<Family>(Family::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo) {}
// clang-format off
#include "shared/source/image/image_skl_plus.inl"
// clang-format on
} // namespace NEO
