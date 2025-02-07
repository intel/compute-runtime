/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"

#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/utilities/io_functions.h"

namespace NEO {

const std::string neoCachePersistent = "NEO_CACHE_PERSISTENT";
const std::string neoCacheMaxSize = "NEO_CACHE_MAX_SIZE";
const std::string neoCacheDir = "NEO_CACHE_DIR";

const int64_t neoCacheMaxSizeDefault = static_cast<int64_t>(1 * 1024 * 1024 * 1024); // 1 Gigabyte

int64_t getSetting(const char *settingName, int64_t defaultValue) {
    int64_t value = defaultValue;
    char *envValue;

    envValue = IoFunctions::getenvPtr(settingName);
    if (envValue) {
        value = atoll(envValue);
        return value;
    }

    return value;
}

std::string getSetting(const char *settingName, const std::string &value) {
    std::string keyValue = value;
    char *envValue = IoFunctions::getEnvironmentVariable(settingName);
    if (envValue) {
        keyValue.assign(envValue);
    }

    return keyValue;
}

CompilerCacheConfig getDefaultCompilerCacheConfig() {
    CompilerCacheConfig ret;
    int64_t compilerCacheDefaultEnabled = 0;

    if (NEO::getSetting(neoCachePersistent.c_str(), compilerCacheDefaultEnabled) != 0) {
        ret.enabled = true;
        std::string emptyString = "";
        ret.cacheDir = NEO::getSetting(neoCacheDir.c_str(), emptyString);

        if (ret.cacheDir.empty()) {
            ret.enabled = false;
            return ret;
        }

        ret.cacheFileExtension = ".ocloc_cache";
        ret.cacheSize = static_cast<size_t>(NEO::getSetting(neoCacheMaxSize.c_str(), neoCacheMaxSizeDefault));

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
