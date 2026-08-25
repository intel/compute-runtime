/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/source/release_helpers/release_helper/release_helper_base.inl"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release1261;

} // namespace NEO

#include "shared/source/release_helpers/release_helper/release_helper_preferred_slm_xe_hpc_128k.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
