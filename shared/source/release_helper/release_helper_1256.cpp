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
constexpr auto release = ReleaseType::release1256;

template <>
bool ReleaseHelperHw<release>::isDummyBlitWaRequired() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isSlmLimitationTo96KNeeded() const {
    return true;
}

} // namespace NEO
#include "shared/source/release_helper/release_helper_common_xe_hpg.inl"
#include "shared/source/release_helper/release_helper_preferred_slm_xe_hpg_g11.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
