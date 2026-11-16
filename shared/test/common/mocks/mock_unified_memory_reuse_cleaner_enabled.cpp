/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_reuse_cleaner.h"

namespace NEO {
bool mockUnifiedMemoryReuseCleanerSupported = false;

bool UnifiedMemoryReuseCleaner::isSupported() {
    return mockUnifiedMemoryReuseCleanerSupported;
}
} // namespace NEO
