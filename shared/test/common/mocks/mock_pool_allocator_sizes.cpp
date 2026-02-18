/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/utilities/pool_allocator_traits.h"

namespace NEO {
size_t CommandBufferPoolTraits::getDefaultPoolSize() {
    return MemoryConstants::pageSize2M;
}
} // namespace NEO
