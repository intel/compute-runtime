/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/page_fault_manager/mock_cpu_page_fault_manager.h"

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

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModel>());
    EXPECT_EQ(exportableMemoryFalse, contextImp->isShareableMemory(nullptr, exportableMemoryFalse, neoDevice));
    EXPECT_EQ(exportableMemoryTrue, contextImp->isShareableMemory(nullptr, exportableMemoryTrue, neoDevice));
    // exportDesc set && neoDevice is NOT WDDM
    EXPECT_EQ(exportableMemoryFalse, contextImp->isShareableMemory(&desc, exportableMemoryFalse, neoDevice));
    // exportDesc unset && neoDevice is NOT WDDM
    EXPECT_EQ(exportableMemoryFalse, contextImp->isShareableMemory(nullptr, exportableMemoryFalse, neoDevice));
    // exportDesc unset && neoDevice is WDDM
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    EXPECT_EQ(exportableMemoryTrue, contextImp->isShareableMemory(nullptr, exportableMemoryFalse, neoDevice));
    // exportDesc is set && Exportable Memory is False && neoDevice is WDDM
    EXPECT_EQ(exportableMemoryFalse, contextImp->isShareableMemory(&desc, exportableMemoryFalse, neoDevice));

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

using GetMemHandlePtrTest = Test<GetMemHandlePtrTestFixture>;
TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithValidNTHandleThenSuccessIsReturned) {
    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);

    uint64_t handle = 57;

    // Test Successfully returning NT Handle
    fixtureMemoryManager->NTHandle = true;
    EXPECT_NE(nullptr, context->getMemHandlePtr(device, handle, 0));
}

TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithInvalidHandleThenNullptrIsReturned) {
    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);

    uint64_t handle = 57;

    driverHandle->failHandleLookup = true;

    // Test Failing returning NT Handle
    fixtureMemoryManager->NTHandle = true;
    EXPECT_EQ(nullptr, context->getMemHandlePtr(device, handle, 0));

    // Test Failing returning fd Handle
    fixtureMemoryManager->NTHandle = false;
    EXPECT_EQ(nullptr, context->getMemHandlePtr(device, handle, 0));
}

TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithDRMDriverTypeWithNonNTHandleThenSuccessIsReturned) {
    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    uint64_t handle = 57;

    // Test Successfully returning fd Handle
    fixtureMemoryManager->NTHandle = false;
    EXPECT_NE(nullptr, context->getMemHandlePtr(device, handle, 0));
}

TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithWDDMDriverTypeWithNonNTHandleThenNullPtrIsReturned) {
    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    uint64_t handle = 57;

    // Test Successfully returning fd Handle
    fixtureMemoryManager->NTHandle = false;
    EXPECT_EQ(nullptr, context->getMemHandlePtr(device, handle, 0));
}

} // namespace ult
} // namespace L0
