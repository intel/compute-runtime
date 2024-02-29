/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"

#include "shared/source/compiler_interface/os_compiler_cache_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/utilities/debug_settings_reader.h"

#include <string>

namespace NEO {

const std::string neoCachePersistent = "NEO_CACHE_PERSISTENT";
const std::string neoCacheMaxSize = "NEO_CACHE_MAX_SIZE";
const std::string neoCacheDir = "NEO_CACHE_DIR";

const int64_t neoCacheMaxSizeDefault = static_cast<int64_t>(MemoryConstants::gigaByte);

CompilerCacheConfig getDefaultCompilerCacheConfig() {
    CompilerCacheConfig ret;
    NEO::EnvironmentVariableReader envReader;

    if (envReader.getSetting(neoCachePersistent.c_str(), ApiSpecificConfig::compilerCacheDefaultEnabled()) != 0) {
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

        ret.cacheFileExtension = ApiSpecificConfig::compilerCacheFileExtension();
        ret.cacheSize = static_cast<size_t>(envReader.getSetting(neoCacheMaxSize.c_str(), neoCacheMaxSizeDefault));

        if (ret.cacheSize == 0u) {
            ret.cacheSize = std::numeric_limits<size_t>::max();
        }

        return ret;
    }

    ret.cacheDir = envReader.getSetting(ApiSpecificConfig::compilerCacheDir().c_str(), ApiSpecificConfig::compilerCacheLocation());

    if (NEO::SysCalls::pathExists(ret.cacheDir)) {
        ret.enabled = true;
        ret.cacheSize = static_cast<size_t>(neoCacheMaxSizeDefault);
        ret.cacheFileExtension = ApiSpecificConfig::compilerCacheFileExtension();
    } else {
        ret.enabled = false;
        ret.cacheSize = 0u;
        ret.cacheFileExtension = ApiSpecificConfig::compilerCacheFileExtension();
    }

    return ret;
}

} // namespace NEO
