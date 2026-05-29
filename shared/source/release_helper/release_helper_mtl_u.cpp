/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "neo_aot_platforms.h"
#include "release_definitions.h"

#include <algorithm>

namespace NEO {
constexpr auto release = ReleaseType::release1270;

template <>
bool ReleaseHelperHw<release>::isPipeControlPriorToNonPipelinedStateCommandsWARequired() const {
    return hardwareIpVersion.value == AOT::MTL_U_A0;
}

template <>
bool ReleaseHelperHw<release>::isProgramAllStateComputeCommandFieldsWARequired() const {
    return hardwareIpVersion.value == AOT::MTL_U_A0;
}

template <>
inline bool ReleaseHelperHw<release>::isAuxSurfaceModeOverrideRequired() const {
    return true;
}

template <>
inline bool ReleaseHelperHw<release>::isDotProductAccumulateSystolicSupported() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isDummyBlitWaRequired() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isMatrixMultiplyAccumulateSupported() const {
    return false;
}

} // namespace NEO
#include "shared/source/release_helper/release_helper_common_xe_lpg.inl"
#include "shared/source/release_helper/release_helper_preferred_slm_xe_hpg_96k.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
