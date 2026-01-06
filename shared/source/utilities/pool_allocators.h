/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/generic_pool_allocator.h"
#include "shared/source/utilities/pool_allocator_traits.h"

namespace NEO {

using TimestampPoolAllocator = GenericPoolAllocator<TimestampPoolTraits>;
using GlobalSurfacePoolAllocator = GenericPoolAllocator<GlobalSurfacePoolTraits>;
using ConstantSurfacePoolAllocator = GenericPoolAllocator<ConstantSurfacePoolTraits>;

} // namespace NEO