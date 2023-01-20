/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#define L0_CACHE_LOCATION "l0_cache"

#include "shared/source/compiler_interface/default_cache_config.h"

#include "shared/source/utilities/debug_settings_reader.h"

#include "level_zero/core/source/compiler_interface/l0_reg_path.h"

#include <string>

namespace NEO {
CompilerCacheConfig getDefaultCompilerCacheConfig() {
    NEO::CompilerCacheConfig ret;

    std::string keyName = L0::registryPath;
    keyName += "l0_cache_dir";
    std::unique_ptr<NEO::SettingsReader> settingsReader(NEO::SettingsReader::createOsReader(false, keyName));
    ret.cacheDir = settingsReader->getSetting(settingsReader->appSpecificLocation(keyName), static_cast<std::string>(L0_CACHE_LOCATION));

    ret.cacheFileExtension = ".l0_cache";

    return ret;
}

} // namespace NEO
