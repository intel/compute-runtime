/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"

#include "opencl/source/compiler_interface/default_cl_cache_config.h"

namespace NEO {
CompilerCacheConfig getDefaultCompilerCacheConfig() {
    return getDefaultClCompilerCacheConfig();
}

} // namespace NEO
