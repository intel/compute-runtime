/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/pool_allocator_traits.h"

namespace NEO {
size_t CommandBufferPoolTraits::getDefaultPoolSize() {
    return defaultPoolSize;
}
} // namespace NEO
