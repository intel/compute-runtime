/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release2001;

template <>
inline bool ReleaseHelperHw<release>::isAuxSurfaceModeOverrideRequired() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isLocalOnlyAllowed() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isBindlessAddressingDisabled() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::programmAdditionalStallPriorToBarrierWithTimestamp() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::shouldQueryPeerAccess() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isSingleDispatchRequiredForMultiCCS() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isStateCacheInvalidationWaRequired() const {
    auto enableStateCacheInvalidationWa = debugManager.flags.EnableStateCacheInvalidationWa.get();
    if (enableStateCacheInvalidationWa != -1) {
        return enableStateCacheInvalidationWa;
    }
    return true;
}

} // namespace NEO

#include "shared/source/release_helper/release_helper_common_xe2.inl"
#include "shared/source/release_helper/release_helper_preferred_slm_xe2_hpg_160k.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
