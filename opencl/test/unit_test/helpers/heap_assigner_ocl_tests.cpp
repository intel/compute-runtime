/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_assigner.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

namespace NEO {

using AlocationHelperTests = Test<ClDeviceFixture>;

HWTEST_F(AlocationHelperTests, givenLinearStreamTypeWhenUseExternalAllocatorForSshAndDshDisabledThenUse32BitIsFalse) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
    HeapAssigner heapAssigner = {};
    EXPECT_FALSE(heapAssigner.use32BitHeap(AllocationType::LINEAR_STREAM));
}

} // namespace NEO