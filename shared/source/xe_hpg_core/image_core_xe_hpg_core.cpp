/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/image/image_surface_state.h"
#include "shared/source/xe_hpg_core/hw_cmds_base.h"

namespace NEO {

using Family = XE_HPG_COREFamily;

template <>
void setFilterMode<Family>(Family::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo) {
    surfaceState->setDisallowLowQualityFiltering(false);
}
// clang-format off
#include "shared/source/image/image_skl_and_later.inl"
// clang-format on
} // namespace NEO
