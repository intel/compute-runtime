/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unified_memory/unified_memory.h"
#include "core/unified_memory/usm_memory_support.h"
#include "public/cl_ext_private.h"

#include "gtest/gtest.h"

TEST(UnifiedMemoryTests, givenCLUSMMemorySupportFlagsWhenUsingUnifiedMemorySupportFlagsThenEverythingMatch) {
    static_assert(CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL == UNIFIED_SHARED_MEMORY_ACCESS, "Flags value difference");
    static_assert(CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL == UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS, "Flags value difference");
    static_assert(CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS_INTEL == UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS, "Flags value difference");
    static_assert(CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS_INTEL == UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS, "Flags value difference");
}
