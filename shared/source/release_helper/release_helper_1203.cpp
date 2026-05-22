/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release1203;

} // namespace NEO

#include "shared/source/release_helper/release_helper_common_gen12lp.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
