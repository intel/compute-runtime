/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"

#include "shared/source/compiler_interface/os_compiler_cache_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/utilities/debug_settings_reader.h"

#include "config.h"
#include "os_inc.h"

#include <string>

namespace NEO {
std::string neoCachePersistent = "NEO_CACHE_PERSISTENT";
std::string neoCacheMaxSize = "NEO_CACHE_MAX_SIZE";
std::string neoCacheDir = "NEO_CACHE_DIR";
std::string clCacheDir = "cl_cache_dir";

CompilerCacheConfig getDefaultCompilerCacheConfig() {
    CompilerCacheConfig ret;
    NEO::EnvironmentVariableReader envReader;

    if (envReader.getSetting(neoCachePersistent.c_str(), defaultCacheEnabled()) != 0) {
        ret.enabled = true;
        std::string emptyString = "";
        ret.cacheDir = envReader.getSetting(neoCacheDir.c_str(), emptyString);

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

        ret.cacheFileExtension = ".cl_cache";
        ret.cacheSize = static_cast<size_t>(envReader.getSetting(neoCacheMaxSize.c_str(), static_cast<int64_t>(MemoryConstants::gigaByte)));

        if (ret.cacheSize == 0u) {
            ret.cacheSize = std::numeric_limits<size_t>::max();
        }

        return ret;
    }

    ret.cacheDir = envReader.getSetting(clCacheDir.c_str(), static_cast<std::string>(CL_CACHE_LOCATION));

    if (NEO::SysCalls::pathExists(ret.cacheDir)) {
        ret.enabled = true;
        ret.cacheSize = MemoryConstants::gigaByte;
        ret.cacheFileExtension = ".cl_cache";
    } else {
        ret.enabled = false;
        ret.cacheSize = 0u;
        ret.cacheFileExtension = ".cl_cache";
    }

    return ret;
}

} // namespace NEO
