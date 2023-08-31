/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"

#include "shared/source/compiler_interface/os_compiler_cache_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/utilities/debug_settings_reader.h"

#include "opencl/source/os_interface/ocl_reg_path.h"

#include "config.h"
#include "os_inc.h"

#include <string>

namespace NEO {
std::string NeoCachePersistent = "NEO_CACHE_PERSISTENT";
std::string NeoCacheMaxSize = "NEO_CACHE_MAX_SIZE";
std::string NeoCacheDir = "NEO_CACHE_DIR";
std::string ClCacheDir = "cl_cache_dir";

CompilerCacheConfig getDefaultCompilerCacheConfig() {
    CompilerCacheConfig ret;

    std::unique_ptr<SettingsReader> settingsReader(SettingsReader::createOsReader(false, oclRegPath));

    auto cachePersistentKey = oclRegPath + NeoCachePersistent;

    if (settingsReader->getSetting(settingsReader->appSpecificLocation(cachePersistentKey), defaultCacheEnabled()) != 0) {
        ret.enabled = true;
        std::string emptyString = "";
        ret.cacheDir = settingsReader->getSetting(settingsReader->appSpecificLocation(NeoCacheDir), emptyString);

        if (ret.cacheDir.empty()) {
            if (!checkDefaultCacheDirSettings(ret.cacheDir, settingsReader.get())) {
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
        ret.cacheSize = static_cast<size_t>(settingsReader->getSetting(settingsReader->appSpecificLocation(NeoCacheMaxSize), static_cast<int64_t>(MemoryConstants::gigaByte)));

        if (ret.cacheSize == 0u) {
            ret.cacheSize = std::numeric_limits<size_t>::max();
        }

        return ret;
    }

    ret.cacheDir = settingsReader->getSetting(settingsReader->appSpecificLocation(ClCacheDir), static_cast<std::string>(CL_CACHE_LOCATION));

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
