/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#define L0_CACHE_LOCATION "l0_cache"

#include "shared/source/compiler_interface/default_cache_config.h"

#include "shared/source/compiler_interface/os_compiler_cache_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/utilities/debug_settings_reader.h"

#include "level_zero/core/source/compiler_interface/l0_reg_path.h"

#include <string>

namespace NEO {
std::string l0CacheDir = "l0_cache_dir";

CompilerCacheConfig getDefaultCompilerCacheConfig() {
    CompilerCacheConfig ret;
    NEO::EnvironmentVariableReader envReader;

    auto neoCachePersistent = debugManager.flags.NEO_CACHE_PERSISTENT.get();
    if (neoCachePersistent == -1) {
        neoCachePersistent = defaultCacheEnabled();
    }

    auto neoCacheDir = debugManager.flags.NEO_CACHE_DIR.get();
    if (neoCacheDir.compare("default") == 0) {
        neoCacheDir = "";
    }

    auto neoCacheMaxSize = debugManager.flags.NEO_CACHE_MAX_SIZE.get();
    if (neoCacheMaxSize == -1) {
        neoCacheMaxSize = MemoryConstants::gigaByte;
    }

    if (neoCachePersistent != 0) {
        ret.enabled = true;
        ret.cacheDir = neoCacheDir;

        if (ret.cacheDir.empty()) {
            if (!checkDefaultCacheDirSettings(ret.cacheDir, envReader)) {
                ret.enabled = false;
                return ret;
            }
        } else {
            if (!NEO::SysCalls::pathExists(ret.cacheDir)) {
                ret.cacheDir = "";
                ret.enabled = false;
                return ret;
            }
        }

        ret.cacheFileExtension = ".l0_cache";
        ret.cacheSize = static_cast<size_t>(neoCacheMaxSize);

        if (ret.cacheSize == 0u) {
            ret.cacheSize = std::numeric_limits<size_t>::max();
        }

        return ret;
    }

    ret.cacheDir = envReader.getSetting(l0CacheDir.c_str(), static_cast<std::string>(L0_CACHE_LOCATION));

    if (NEO::SysCalls::pathExists(ret.cacheDir)) {
        ret.enabled = true;
        ret.cacheSize = MemoryConstants::gigaByte;
        ret.cacheFileExtension = ".l0_cache";
    } else {
        ret.enabled = false;
        ret.cacheSize = 0u;
        ret.cacheFileExtension = ".l0_cache";
    }

    return ret;
}

} // namespace NEO
