/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

namespace NEO {

template <>
bool CompilerReleaseHelperHw<release>::isBindlessAddressingDisabled() const {
    return false;
}

template <>
bool CompilerReleaseHelperHw<release>::isSplitMatrixMultiplyAccumulateSupported() const {
    return true;
}

} // namespace NEO
