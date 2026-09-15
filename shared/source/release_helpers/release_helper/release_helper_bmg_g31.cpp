/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/source/release_helpers/release_helper/release_helper_base.inl"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release2002;

} // namespace NEO

#include "shared/source/release_helpers/release_helper/release_helper_common_xe2.inl"
#include "shared/source/release_helpers/release_helper/release_helper_preferred_slm_xe2_hpg_160k.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
