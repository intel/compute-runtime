/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/compiler_interface/default_cache_config.h"

#include "runtime/compiler_interface/default_cl_cache_config.h"

namespace NEO {
CompilerCacheConfig getDefaultCompilerCacheConfig() {
    return getDefaultClCompilerCacheConfig();
}

} // namespace NEO
