/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/required_libs_helpers.h"

#include "shared/source/utilities/io_functions.h"

#include <algorithm>
#include <array>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

namespace NEO {

const std::span<const std::string_view> RequiredLibsHelpers::getDefaultBinarySearchPaths() {
    using namespace std::string_view_literals;
    static constexpr auto fixedPaths = std::to_array({"/lib"sv,
                                                      "/lib64"sv,
                                                      "/lib/x86_64-linux-gnu"sv,
                                                      "/usr/lib"sv,
                                                      "/usr/lib64"sv,
                                                      "/usr/lib/x86_64-linux-gnu"sv,
                                                      "/usr/local/lib"sv});
    return fixedPaths;
}

const std::span<const std::string_view> RequiredLibsHelpers::getOptionalBinarySearchPaths(LockableSearchPaths &optionalPathsCache) {
    auto lock = optionalPathsCache.lock();

    if (not optionalPathsCache->empty()) {
        return *optionalPathsCache;
    }

    const char *ldPathsStr = NEO::IoFunctions::getEnvironmentVariable("LD_LIBRARY_PATH");
    if (nullptr == ldPathsStr) {
        return {};
    }
    auto ldPathsView = std::string_view(ldPathsStr);
    if (ldPathsView.empty()) {
        return {};
    }

    optionalPathsCache->reserve(std::ranges::count(ldPathsView, ':') + 1);
    std::ranges::for_each(
        ldPathsView | std::views::split(':'),
        [&](auto &&sv) { optionalPathsCache->push_back(sv); },
        [](auto &&rng) { return std::string_view(&*rng.begin(), std::ranges::distance(rng)); });

    return *optionalPathsCache;
}

} // namespace NEO
