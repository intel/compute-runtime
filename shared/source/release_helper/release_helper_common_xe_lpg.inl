/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <>
bool ReleaseHelperHw<release>::isMatrixMultiplyAccumulateSupported() const {
    return false;
}

} // namespace NEO
