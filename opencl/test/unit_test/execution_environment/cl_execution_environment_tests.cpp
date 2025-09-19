/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/mocks/mock_async_event_handler.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

TEST(ExecutionEnvironment, givenPlatformWhenItIsConstructedThenItCretesExecutionEnvironmentWithOneRefCountInternal) {
    auto executionEnvironment = new ExecutionEnvironment();
    EXPECT_EQ(0, executionEnvironment->getRefInternalCount());

    std::unique_ptr<Platform> platform(new Platform(*executionEnvironment));
    EXPECT_EQ(executionEnvironment, platform->peekExecutionEnvironment());
    EXPECT_EQ(1, executionEnvironment->getRefInternalCount());
}

TEST(ExecutionEnvironment, givenPlatformAndExecutionEnvironmentWithRefCountsWhenPlatformIsDestroyedThenExecutionEnvironmentIsNotDeleted) {
    auto executionEnvironment = new ExecutionEnvironment();
    std::unique_ptr<Platform> platform(new Platform(*executionEnvironment));
    executionEnvironment->incRefInternal();
    platform.reset();
    EXPECT_EQ(1, executionEnvironment->getRefInternalCount());
    executionEnvironment->decRefInternal();
}

TEST(ExecutionEnvironment, givenDeviceThatHaveReferencesAfterPlatformIsDestroyedThenDeviceIsStillUsable) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(1);
    auto executionEnvironment = new ExecutionEnvironment();
    std::unique_ptr<Platform> platform(new Platform(*executionEnvironment));
    platform->initialize(DeviceFactory::createDevices(*executionEnvironment));
    auto device = platform->getClDevice(0);
    EXPECT_EQ(1, device->getRefInternalCount());
    device->incRefInternal();
    platform.reset(nullptr);
    EXPECT_EQ(1, device->getRefInternalCount());

    int32_t expectedRefCount = 1 + device->getNumSubDevices();

    EXPECT_EQ(expectedRefCount, executionEnvironment->getRefInternalCount());

    device->decRefInternal();
}

TEST(ExecutionEnvironment, givenPlatformWhenItIsCreatedThenItCreatesMemoryManagerInExecutionEnvironment) {
    auto executionEnvironment = new ExecutionEnvironment();
    Platform platform(*executionEnvironment);
    prepareDeviceEnvironments(*executionEnvironment);
    platform.initialize(DeviceFactory::createDevices(*executionEnvironment));
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(ClExecutionEnvironment, WhenExecutionEnvironmentIsDeletedThenAsyncEventHandlerThreadIsDestroyed) {
    auto executionEnvironment = new MockClExecutionEnvironment();
    MockHandler *mockAsyncHandler = new MockHandler();

    executionEnvironment->asyncEventsHandler.reset(mockAsyncHandler);
    EXPECT_EQ(mockAsyncHandler, executionEnvironment->getAsyncEventsHandler());

    mockAsyncHandler->openThread();
    delete executionEnvironment;
    EXPECT_TRUE(MockAsyncEventHandlerGlobals::destructorCalled);
}

TEST(ClExecutionEnvironment, WhenExecutionEnvironmentIsCreatedThenAsyncEventHandlerIsCreated) {
    auto executionEnvironment = std::make_unique<ClExecutionEnvironment>();
    EXPECT_NE(nullptr, executionEnvironment->getAsyncEventsHandler());
}
