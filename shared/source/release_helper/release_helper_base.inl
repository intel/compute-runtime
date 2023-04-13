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

} // namespace NEO
