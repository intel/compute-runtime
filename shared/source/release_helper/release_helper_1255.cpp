/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"

#include "platforms.h"
#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release1255;

template <>
bool ReleaseHelperHw<release>::isPrefetchDisablingRequired() const {

    return hardwareIpVersion.value < AOT::DG2_G10_B0;
}

} // namespace NEO
#include "shared/source/release_helper/release_helper_common_xe_hpg.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
