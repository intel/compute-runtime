/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/lockable.h"

#include <span>
#include <string>
#include <vector>

namespace NEO {

struct MockRequiredLibsHelpers {
    static inline std::vector<std::string_view> defaultSearchPaths{};
    static inline constinit int getDefaultBinarySearchPathsCalled = 0;
    static const std::span<const std::string_view> getDefaultBinarySearchPaths() {
        ++getDefaultBinarySearchPathsCalled;
        return defaultSearchPaths;
    }

    static inline std::vector<std::string_view> optionalSearchPaths{};
    static inline constinit int getOptionalBinarySearchPathsCalled = 0;
    static const std::span<const std::string_view> getOptionalBinarySearchPaths(NEO::Lockable<std::vector<std::string_view>> &optionalPathsCache) {
        ++getOptionalBinarySearchPathsCalled;
        return optionalSearchPaths;
    }
};

} // namespace NEO
