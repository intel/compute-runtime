/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "default_cl_cache_config.h"

#include "core/utilities/debug_settings_reader.h"
#include "runtime/os_interface/ocl_reg_path.h"

#include "config.h"
#include "os_inc.h"

#include <string>

namespace NEO {

CompilerCacheConfig getDefaultClCompilerCacheConfig() {
    CompilerCacheConfig ret;

    std::string keyName = oclRegPath;
    keyName += "cl_cache_dir";
    std::unique_ptr<SettingsReader> settingsReader(SettingsReader::createOsReader(false, keyName));
    ret.cacheDir = settingsReader->getSetting(settingsReader->appSpecificLocation(keyName), static_cast<std::string>(CL_CACHE_LOCATION));

    ret.cacheFileExtension = ".cl_cache";

    return ret;
}

} // namespace NEO
