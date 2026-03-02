/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/path.h"
#include "shared/source/utilities/lockable.h"

#include <algorithm>
#include <array>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

namespace NEO {

struct RequiredLibsHelpers {
    using LockableSearchPaths = Lockable<std::vector<std::string_view>>;

    static const std::span<const std::string_view> getDefaultBinarySearchPaths();
    static const std::span<const std::string_view> getOptionalBinarySearchPaths(LockableSearchPaths &optionalPathsCache);

    template <typename ReqLibsHelpersT = RequiredLibsHelpers>
    static bool getRequiredLibDirPath(const std::string &libName,
                                      LockableSearchPaths &cachedSearchPaths,
                                      std::string &outDirPath) {
        if (const auto customPathStr = NEO::debugManager.flags.RequiredLibsBinarySearchPath.get(); customPathStr != "none") {
            if (not fileExists(NEO::joinPath(customPathStr, libName))) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "The binary: %s not found in: %s\n", libName.c_str(), customPathStr.c_str());
                return false;
            }
            outDirPath = customPathStr;
            return true;
        } else {
            auto findLibInPaths = [&libName](const auto &paths) {
                return std::ranges::find_if(paths, [libName](const auto dirPath) {
                    return fileExists(NEO::joinPath(std::string{dirPath}, libName));
                });
            };

            auto optionalSearchPaths = ReqLibsHelpersT::getOptionalBinarySearchPaths(cachedSearchPaths);
            if (not optionalSearchPaths.empty()) {
                auto pathIt = findLibInPaths(optionalSearchPaths);
                if (pathIt != optionalSearchPaths.end()) {
                    outDirPath = std::string(*pathIt);
                    return true;
                }
            }

            auto defaultSearchPaths = ReqLibsHelpersT::getDefaultBinarySearchPaths();
            auto pathIt = findLibInPaths(defaultSearchPaths);
            if (pathIt == defaultSearchPaths.end()) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "The binary: %s not found in standard locations\n", libName.c_str());
                return false;
            }
            outDirPath = std::string(*pathIt);
            return true;
        }
    }
};

} // namespace NEO
