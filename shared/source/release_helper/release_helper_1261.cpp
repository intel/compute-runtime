/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release1261;

template <>
bool ReleaseHelperHw<release>::isRcsExposureDisabled() const {
    return true;
}

template <>
inline bool ReleaseHelperHw<release>::isDotProductAccumulateSystolicSupported() const {
    return false;
}

template <>
inline bool ReleaseHelperHw<release>::isMatrixMultiplyAccumulateSupported() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isDummyBlitWaRequired() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::getFtrXe2Compression() const {
    return false;
}

} // namespace NEO

#include "shared/source/release_helper/release_helper_preferred_slm_xe_hpc_128k.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
