/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"
namespace L0 {
namespace ult {

using ContextLockMemoryTests = Test<HostPointerManagerFixure>;

TEST_F(ContextLockMemoryTests, givenValidPointerWhenCallingLockMemoryThenUnsupportedErrorIsReturned) {
    const size_t size = 4096;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t res = context->allocDeviceMem(device->toHandle(),
                                              &deviceDesc,
                                              size,
                                              0,
                                              &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    mockMemoryInterface->lockResult = NEO::MemoryOperationsStatus::success;
    res = context->lockMemory(device, ptr, size);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    context->freeMem(ptr);
}

} // namespace ult
} // namespace L0
