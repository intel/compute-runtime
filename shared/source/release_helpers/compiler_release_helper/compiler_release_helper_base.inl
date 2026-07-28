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

} // namespace NEO
