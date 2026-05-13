/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using InitOpaqueHandleResourcesWindowsTest = Test<DeviceFixture>;

TEST_F(InitOpaqueHandleResourcesWindowsTest, GivenWindowsWhenCallingInitOpaqueHandleResourcesThenNoOpSucceeds) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    Context *contextImp = Context::fromHandle(hContext);

    // initOpaqueHandleResources is a no-op on Windows - verify it does not crash
    contextImp->initOpaqueHandleResources();
    contextImp->initOpaqueHandleResources(); // calling twice is also safe

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

} // namespace ult
} // namespace L0
