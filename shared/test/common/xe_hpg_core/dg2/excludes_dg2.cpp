/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(CommandEncodeStatesTest, givenSlmTotalSizeEqualZeroWhenDispatchingKernelThenSharedMemorySizeIsSetCorrectly, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(MemoryManagerTests, givenEnabledLocalMemoryWhenAllocateInternalAllocationInDevicePoolThen32BitAllocationIsCreated, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(MemoryManagerTests, givenEnabledLocalMemoryWhenLinearStreamIsAllocatedInDevicePoolThenLocalMemoryPoolIsUsed, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(MemoryManagerTests, givenEnabledLocalMemoryWhenAllocateKernelIsaInDevicePoolThenLocalMemoryPoolIsUsed, IGFX_DG2);
