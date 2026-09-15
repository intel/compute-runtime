/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

namespace NEO {

CompilerCacheConfig getDefaultCompilerCacheConfig() {
    CompilerCacheConfig ret;
    auto &flags = NEO::debugManager.flags;

    if (flags.EnvCachePersistent.get() == 1) {
        ret.enabled = true;

        ret.statsEnabled = flags.EnvCacheStats.get();

        ret.cacheDir = flags.EnvCacheDir.get();

        if (ret.cacheDir.empty()) {
            ret.enabled = false;
            ret.statsEnabled = false;
            return ret;
        }

        ret.cacheFileExtension = ".ocloc_cache";
        ret.cacheSize = static_cast<size_t>(flags.EnvCacheMaxSize.get());

        if (ret.cacheSize == 0u) {
            ret.cacheSize = std::numeric_limits<size_t>::max();
        }

        return ret;
    }

    ret.cacheDir = "ocloc_cache";
    ret.cacheFileExtension = ".ocloc_cache";

    return ret;
}
} // namespace NEO
