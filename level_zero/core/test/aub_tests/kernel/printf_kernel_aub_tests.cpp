/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/aub_tests/kernel/kernel_aub_fixture.h"

namespace L0 {
namespace ult {

using AUBPrintfKernelTestL0 = Test<KernelAubFixtureL0>;

HWTEST_F(AUBPrintfKernelTestL0, givenPrintfKernelWhenLaunchedThenExecutionSucceeds) {
    auto kernel = this->createKernel("simple_kernels", "test_printf_number", true);
    ASSERT_NE(nullptr, kernel);
    EXPECT_NE(nullptr, toKernel(kernel)->getPrintfBufferAllocation());

    auto buffer = static_cast<uint32_t *>(this->allocateHostMemory(MemoryConstants::pageSize, MemoryConstants::pageSize));
    ASSERT_NE(nullptr, buffer);
    const uint32_t number = 4;
    buffer[0] = number;
    this->writeHostMemory<FamilyType>(buffer);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &buffer));

    const uint32_t groupSize[3] = {4, 1, 1};
    const uint32_t groupCount[3] = {1, 1, 1};
    this->appendLaunch(kernel, groupSize, groupCount);
    this->executeAndSynchronize();

    this->expectMemory<FamilyType>(this->getGpuPointer(buffer), &number, sizeof(number));
}

} // namespace ult
} // namespace L0
