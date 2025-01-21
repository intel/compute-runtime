/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/unified_memory_reuse_cleaner.h"
namespace NEO {
struct MockUnifiedMemoryReuseCleaner : public UnifiedMemoryReuseCleaner {
  public:
    using UnifiedMemoryReuseCleaner::svmAllocationCaches;
    using UnifiedMemoryReuseCleaner::trimOldInCaches;
    void startThread() override{};
};
} // namespace NEO