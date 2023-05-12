/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isMatrixMultiplyAccumulateSupported() const {
    return true;
}
template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isAdjustWalkOrderAvailable() const {
    return false;
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isPipeControlPriorToNonPipelinedStateCommandsWARequired() const {
    return false;
}

} // namespace NEO
