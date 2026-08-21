/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

namespace NEO {

template <>
bool CompilerReleaseHelperHw<release>::getFtrXe2Compression() const {
    return false;
}

} // namespace NEO
