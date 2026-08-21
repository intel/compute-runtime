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
bool CompilerReleaseHelperHw<releaseType>::isMatrixMultiplyAccumulateSupported() const {
    return true;
}

template <ReleaseType releaseType>
uint32_t CompilerReleaseHelperHw<releaseType>::getAdditionalFp16Caps() const {
    return 0u;
}

template <ReleaseType releaseType>
uint32_t CompilerReleaseHelperHw<releaseType>::getAdditionalExtraCaps() const {
    return 0u;
}

template <ReleaseType releaseType>
bool CompilerReleaseHelperHw<releaseType>::getFtrXe2Compression() const {
    return true;
}

template <ReleaseType releaseType>
bool CompilerReleaseHelperHw<releaseType>::isAvailableSemaphore64Base() const {
    return false;
}

} // namespace NEO
