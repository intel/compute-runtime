/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/pool_allocators.h"

#include "shared/source/utilities/generic_pool_allocator.inl"

namespace NEO {

#define INSTANTIATE_POOL_ALLOCATOR(Traits)                                            \
    template class AbstractBuffersAllocator<GenericPool<Traits>, GraphicsAllocation>; \
    template class GenericPoolAllocator<Traits>

INSTANTIATE_POOL_ALLOCATOR(TimestampPoolTraits);
INSTANTIATE_POOL_ALLOCATOR(GlobalSurfacePoolTraits);
INSTANTIATE_POOL_ALLOCATOR(ConstantSurfacePoolTraits);

#undef INSTANTIATE_POOL_ALLOCATOR

} // namespace NEO