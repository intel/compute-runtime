/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"

#include "shared/source/compiler_interface/os_compiler_cache_helper.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/path.h"

#include <string>

namespace NEO {

CompilerCacheConfig getDefaultCompilerCacheConfig() {
    CompilerCacheConfig ret;
    auto &flags = NEO::debugManager.flags;

    if (flags.EnvCachePersistent.get() != 0) {
        ret.enabled = true;

        ret.statsEnabled = flags.EnvCacheStats.get();

        ret.cacheDir = flags.EnvCacheDir.get();

        if (ret.cacheDir.empty()) {
            if (!checkDefaultCacheDirSettings(ret.cacheDir)) {
                ret.enabled = false;
                ret.statsEnabled = false;
                return ret;
            }
        } else {
            if (!NEO::pathExists(ret.cacheDir)) {
                ret.cacheDir = "";
                ret.enabled = false;
                ret.statsEnabled = false;
                return ret;
            }
        }

        ret.cacheFileExtension = ApiSpecificConfig::compilerCacheFileExtension();
        ret.cacheSize = static_cast<size_t>(flags.EnvCacheMaxSize.get());

        if (ret.cacheSize == 0u) {
            ret.cacheSize = std::numeric_limits<size_t>::max();
        }

        PRINT_STRING(flags.PrintDebugMessages.get(), stdout, "NEO_CACHE_PERSISTENT is enabled. Cache is located in: %s\n\n",
                     ret.cacheDir.c_str());

        return ret;
    }

    return ret;
}
} // namespace NEO
