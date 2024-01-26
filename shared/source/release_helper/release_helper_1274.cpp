/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release1274;

template <>
bool ReleaseHelperHw<release>::getMediaFrequencyTileIndex(uint32_t &tileIndex) const {
    tileIndex = 1;
    return true;
}

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
bool ReleaseHelperHw<release>::isSplitMatrixMultiplyAccumulateSupported() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isDirectSubmissionSupported() const {
    return true;
}

} // namespace NEO

template class NEO::ReleaseHelperHw<NEO::release>;
