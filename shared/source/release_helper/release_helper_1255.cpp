/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"

#include "neo_aot_platforms.h"
#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release1255;

} // namespace NEO
#include "shared/source/release_helper/release_helper_common_xe_hpg.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
