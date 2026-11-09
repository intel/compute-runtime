/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_excludes.h"

HWTEST_EXCLUDE_PRODUCT(WddmMemoryManagerSimpleTest, givenLinearStreamWhenItIsAllocatedThenItIsInLocalMemoryHasCpuPointerAndHasStandardHeap64kbAsGpuAddress, IGFX_DG2);
