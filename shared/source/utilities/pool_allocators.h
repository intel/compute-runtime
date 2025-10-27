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

using TimestampPool = GenericPool<TimestampPoolTraits>;
using TimestampPoolAllocator = GenericPoolAllocator<TimestampPoolTraits>;

} // namespace NEO