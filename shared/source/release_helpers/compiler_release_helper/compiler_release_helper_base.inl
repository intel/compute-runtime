/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

namespace NEO {

template <ReleaseType releaseType>
uint32_t CompilerReleaseHelperHw<releaseType>::getAdditionalFp16Caps() const {
    return 0u;
}

template <ReleaseType releaseType>
uint32_t CompilerReleaseHelperHw<releaseType>::getAdditionalExtraCaps() const {
    return 0u;
}

} // namespace NEO
