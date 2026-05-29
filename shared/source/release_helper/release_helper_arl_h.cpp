/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release1274;

template <>
bool ReleaseHelperHw<release>::isAdjustWalkOrderAvailable() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isBFloat16ConversionSupported() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isPipeControlPriorToPipelineSelectWaRequired() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isBlitImageAllowedForDepthFormat() const {
    return false;
}

} // namespace NEO

#include "shared/source/release_helper/release_helper_common_xe_lpg.inl"
#include "shared/source/release_helper/release_helper_preferred_slm_xe_hpg_128k.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
