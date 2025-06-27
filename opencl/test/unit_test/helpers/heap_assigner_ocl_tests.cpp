/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

namespace NEO {

using AlocationHelperTests = Test<ClDeviceFixture>;

HWTEST_F(AlocationHelperTests, givenLinearStreamTypeWhenUseExternalAllocatorForSshAndDshDisabledThenUse32BitIsFalse) {
    HeapAssigner heapAssigner{false};
    EXPECT_FALSE(heapAssigner.use32BitHeap(AllocationType::linearStream));
}

} // namespace NEO