/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"

namespace NEO {
CompilerCacheConfig getDefaultCompilerCacheConfig() {
    CompilerCacheConfig ret;
    ret.cacheDir = "ocloc_cache";
    ret.cacheFileExtension = ".ocloc_cache";
    return ret;
}
} // namespace NEO
