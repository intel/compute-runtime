/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/global_teardown/global_platform_teardown.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

class GlobalPlatformTeardownTest : public ::testing::Test {

    void SetUp() override {
        tmpPlatforms = platformsImpl;
        platformsImpl = nullptr;
    }

    void TearDown() override {
        globalPlatformTeardown(false);
        wasPlatformTeardownCalled = false;
        platformsImpl = tmpPlatforms;
    }
    std::vector<std::unique_ptr<Platform>> *tmpPlatforms;
};

TEST_F(GlobalPlatformTeardownTest, whenCallingPlatformSetupThenPlatformsAllocated) {
    globalPlatformSetup();
    EXPECT_NE(platformsImpl, nullptr);
}
TEST_F(GlobalPlatformTeardownTest, whenCallingPlatformSetupThenWasTeardownCalledIsSetToFalse) {
    globalPlatformSetup();
    EXPECT_FALSE(wasPlatformTeardownCalled);
}
TEST_F(GlobalPlatformTeardownTest, whenCallingPlatformTeardownAndNotTerminatingProcessThenPlatformsDestroyed) {
    globalPlatformSetup();
    EXPECT_NE(platformsImpl, nullptr);

    globalPlatformTeardown(false);
    EXPECT_EQ(platformsImpl, nullptr);
    EXPECT_TRUE(wasPlatformTeardownCalled);
}
TEST_F(GlobalPlatformTeardownTest, whenCallingPlatformTeardownAndTerminatingProcessThenPlatformsNotDestroyed) {
    globalPlatformSetup();
    EXPECT_NE(platformsImpl, nullptr);

    globalPlatformTeardown(true);
    EXPECT_NE(platformsImpl, nullptr);
    EXPECT_TRUE(wasPlatformTeardownCalled);
}

TEST_F(GlobalPlatformTeardownTest, whenCallingPlatformTeardownThenStopDirectSubmissionAndPollForCompletionCalled) {
    VariableBackup<decltype(DeviceFactory::createRootDeviceFunc)> createFuncBackup{&DeviceFactory::createRootDeviceFunc};
    DeviceFactory::createRootDeviceFunc = [](ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) -> std::unique_ptr<Device> {
        return std::unique_ptr<Device>(MockDevice::create<MockDevice>(&executionEnvironment, rootDeviceIndex));
    };

    for (bool processTermination : ::testing::Bool()) {
        globalPlatformSetup();
        platformsImpl->push_back(std::make_unique<MockPlatform>());
        auto pPlatform = static_cast<MockPlatform *>((*platformsImpl)[0].get());
        pPlatform->initializeWithNewDevices();

        auto clDevice = pPlatform->getClDevice(0);
        EXPECT_NE(nullptr, clDevice);
        auto mockDevice = static_cast<MockDevice *>(&clDevice->getDevice());
        mockDevice->stopDirectSubmissionCalled = false;
        mockDevice->pollForCompletionCalled = false;
        clDevice->incRefInternal();

        globalPlatformTeardown(processTermination);
        EXPECT_TRUE(mockDevice->stopDirectSubmissionCalled);
        EXPECT_TRUE(mockDevice->pollForCompletionCalled);

        clDevice->decRefInternal();
    }
}

TEST_F(GlobalPlatformTeardownTest, whenCallingDevicesCleanupMultipleTimesAndNotTerminatingProcessThenItDoesNotCrash) {
    globalPlatformSetup();
    platformsImpl->push_back(std::make_unique<MockPlatform>());
    auto pPlatform = static_cast<MockPlatform *>((*platformsImpl)[0].get());
    pPlatform->initializeWithNewDevices();

    EXPECT_EQ(1u, pPlatform->getNumDevices());
    pPlatform->devicesCleanup(false);
    EXPECT_EQ(0u, pPlatform->getNumDevices());
    pPlatform->devicesCleanup(false);
    EXPECT_EQ(0u, pPlatform->getNumDevices());
}

TEST_F(GlobalPlatformTeardownTest, whenCallingDevicesCleanupMultipleTimesAndTerminatingProcessThenItDoesNotCrash) {
    globalPlatformSetup();
    platformsImpl->push_back(std::make_unique<MockPlatform>());
    auto pPlatform = static_cast<MockPlatform *>((*platformsImpl)[0].get());
    pPlatform->initializeWithNewDevices();

    EXPECT_EQ(1u, pPlatform->getNumDevices());
    pPlatform->devicesCleanup(true);
    EXPECT_EQ(1u, pPlatform->getNumDevices());
    pPlatform->devicesCleanup(true);
    EXPECT_EQ(1u, pPlatform->getNumDevices());
}
