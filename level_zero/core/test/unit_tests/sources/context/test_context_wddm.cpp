/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using ContextIsShareable = Test<DeviceFixture>;
TEST_F(ContextIsShareable, whenCallingisSharedMemoryThenCorrectResultIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    bool exportableMemoryFalse = false;
    bool exportableMemoryTrue = true;
    EXPECT_EQ(exportableMemoryFalse, contextImp->isShareableMemory(nullptr, exportableMemoryFalse, neoDevice, false));
    EXPECT_EQ(exportableMemoryTrue, contextImp->isShareableMemory(nullptr, exportableMemoryTrue, neoDevice, false));

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

using GetMemHandlePtrTest = Test<GetMemHandlePtrTestFixture>;
TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithValidHandleThenSuccessIsReturned) {
    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);

    uint64_t handle = 57;

    // Test Successfully returning NT Handle
    fixtureMemoryManager->ntHandle = true;
    EXPECT_NE(nullptr, context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 0u, 0));
}

TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithInvalidHandleThenNullptrIsReturned) {
    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);

    uint64_t handle = 57;

    driverHandle->failHandleLookup = true;

    // Test Failing returning NT Handle
    fixtureMemoryManager->ntHandle = true;
    EXPECT_EQ(nullptr, context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 0u, 0));
}

} // namespace ult
} // namespace L0
