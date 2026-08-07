/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

namespace NEO {

template <ReleaseType releaseType>
bool CompilerReleaseHelperHw<releaseType>::isForceEmuInt32DivRemSPRequired() const {
    return false;
}

template <ReleaseType releaseType>
bool CompilerReleaseHelperHw<releaseType>::isBindlessAddressingDisabled() const {
    return true;
}

template <ReleaseType releaseType>
bool CompilerReleaseHelperHw<releaseType>::isMatrixMultiplyAccumulateSupported() const {
    return true;
}

template <ReleaseType releaseType>
bool CompilerReleaseHelperHw<releaseType>::isSplitMatrixMultiplyAccumulateSupported() const {
    return false;
}

template <ReleaseType releaseType>
bool CompilerReleaseHelperHw<releaseType>::isBFloat16ConversionSupported() const {
    return true;
}

} // namespace NEO
