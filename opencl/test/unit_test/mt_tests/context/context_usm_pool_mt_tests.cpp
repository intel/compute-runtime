/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_product_helper.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory_with_platform.h"

#include "gtest/gtest.h"

#include <thread>

using namespace NEO;

struct ContextUsmPoolMtTest : public ::testing::Test {
    void SetUp() override {
        device = deviceFactory.rootDevices[0];
        mockNeoDevice = static_cast<MockDevice *>(&device->getDevice());
        mockProductHelper = new MockProductHelper;
        mockNeoDevice->getRootDeviceEnvironmentRef().productHelper.reset(mockProductHelper);
    }

    UltClDeviceFactoryWithPlatform deviceFactory{1, 0};
    MockClDevice *device;
    MockDevice *mockNeoDevice;
    MockProductHelper *mockProductHelper;
    std::unique_ptr<MockContext> context;
    cl_int retVal = CL_SUCCESS;
    DebugManagerStateRestore restore;
};

TEST_F(ContextUsmPoolMtTest, GivenPoolAlreadyInitializedUnderLockWhenInitializeDeviceUsmAllocationPoolThenDoubleCheckPreventsReinit) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeviceUsmAllocationPool.set(1);

    cl_device_id devices[] = {device};
    context.reset(Context::create<MockContext>(nullptr, ClDeviceVector(devices, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    context->takeOwnership();
    std::thread t([&]() {
        context->initializeDeviceUsmAllocationPool();
    });
    while (context->getCond().peekNumWaiters() == 0u) {
        std::this_thread::yield();
    }
    context->usmPoolInitialized = true;
    context->releaseOwnership();
    t.join();
    EXPECT_TRUE(context->usmPoolInitialized);
    context->usmPoolInitialized = false;
}

TEST_F(ContextUsmPoolMtTest, GivenPoolAlreadyInitializedUnderLockWhenInitializeHostUsmAllocationPoolThenDoubleCheckPreventsReinit) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostUsmAllocationPool.set(1);

    cl_device_id devices[] = {device};
    context.reset(Context::create<MockContext>(nullptr, ClDeviceVector(devices, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto platform = static_cast<MockPlatform *>(context->getDevice(0)->getPlatform());
    EXPECT_NE(nullptr, platform);

    platform->takeOwnership();
    std::thread t([&]() {
        platform->initializeHostUsmAllocationPool();
    });
    while (platform->getCond().peekNumWaiters() == 0u) {
        std::this_thread::yield();
    }
    platform->usmPoolInitialized = true;
    platform->releaseOwnership();
    t.join();
    EXPECT_TRUE(platform->usmPoolInitialized);
    platform->usmPoolInitialized = false;
}
