/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/prefetch_manager.h"

using namespace NEO;

class MockPrefetchManager : public PrefetchManager {
  public:
    using PrefetchManager::allocations;
};
