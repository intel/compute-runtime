/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"

#include "level_zero/core/source/compiler_interface/default_l0_cache_config.h"

namespace NEO {
CompilerCacheConfig getDefaultCompilerCacheConfig() {
    return L0::getDefaultL0CompilerCacheConfig();
}

} // namespace NEO
