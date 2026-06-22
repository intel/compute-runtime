/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

namespace NEO {

class CachePolicyOptionHelper {
  public:
    static constexpr const char *storeCachePolicyPrefix = "-cl-store-cache-default=";
    static constexpr const char *loadCachePolicyPrefix = "-cl-load-cache-default=";

    static void replaceCachePolicy(std::string &buildOptions, const char *newCachePolicy) {
        auto cachePolicyStart = buildOptions.find(storeCachePolicyPrefix);
        if (cachePolicyStart == std::string::npos) {
            return;
        }

        // Store and load options always come together, so the policy block ends at the
        // end of the load option's value (or the store value when load is not present).
        auto loadStart = buildOptions.find(loadCachePolicyPrefix, cachePolicyStart);
        size_t valueSearchStart = (loadStart != std::string::npos) ? loadStart : cachePolicyStart;
        size_t cachePolicyEnd = buildOptions.find(' ', valueSearchStart);

        size_t replaceLength = (cachePolicyEnd == std::string::npos) ? std::string::npos : cachePolicyEnd - cachePolicyStart;
        buildOptions.replace(cachePolicyStart, replaceLength, newCachePolicy);
    }
};

} // namespace NEO
