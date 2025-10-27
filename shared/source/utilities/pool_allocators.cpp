/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/pool_allocators.h"

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/generic_pool_allocator.inl"

namespace NEO {

template class GenericPool<TimestampPoolTraits>;
template class GenericPoolAllocator<TimestampPoolTraits>;

static_assert(NEO::NonCopyable<TimestampPool>);

} // namespace NEO