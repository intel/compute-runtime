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
constexpr auto release = ReleaseType::release1270;

template <>
bool ReleaseHelperHw<release>::isPipeControlPriorToNonPipelinedStateCommandsWARequired() const {
    return hardwareIpVersion.value == AOT::MTL_M_A0;
}

} // namespace NEO
#include "shared/source/release_helper/release_helper_common_xe_lpg.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
