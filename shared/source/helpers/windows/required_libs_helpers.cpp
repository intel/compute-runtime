/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/required_libs_helpers.h"

#include <span>
#include <string_view>

namespace NEO {

const std::span<const std::string_view> RequiredLibsHelpers::getDefaultBinarySearchPaths() {
    return {};
}

const std::span<const std::string_view> RequiredLibsHelpers::getOptionalBinarySearchPaths(LockableSearchPaths &optionalPathsCache) {
    return *optionalPathsCache;
}

} // namespace NEO