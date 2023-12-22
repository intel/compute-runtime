/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/unified_memory_pooling.h"

class MockUsmMemAllocPool : public UsmMemAllocPool {
  public:
    using UsmMemAllocPool::allocations;
    using UsmMemAllocPool::pool;
    using UsmMemAllocPool::poolEnd;
    using UsmMemAllocPool::poolMemoryType;
    using UsmMemAllocPool::poolSize;
};