/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory_with_platform.h"

#include "gtest/gtest.h"

namespace NEO {
class MultiRootDeviceFixture : public ::testing::Test {
  public:
    void SetUp() override {
        deviceFactory = std::make_unique<UltClDeviceFactoryWithPlatform>(3, 0);
        device1 = deviceFactory->rootDevices[1];
        device2 = deviceFactory->rootDevices[2];

        cl_device_id devices[] = {device1, device2};

        context.reset(new MockContext(ClDeviceVector(devices, 2), false));
        mockMemoryManager = static_cast<MockMemoryManager *>(device1->getMemoryManager());

        ASSERT_EQ(mockMemoryManager, device1->getMemoryManager());
        for (auto &rootDeviceIndex : context->getRootDeviceIndices()) {
            mockMemoryManager->localMemorySupported[rootDeviceIndex] = true;
        }
    }

    const uint32_t expectedRootDeviceIndex = 1;
    std::unique_ptr<UltClDeviceFactoryWithPlatform> deviceFactory;
    MockClDevice *device1 = nullptr;
    MockClDevice *device2 = nullptr;
    std::unique_ptr<MockContext> context;
    MockMemoryManager *mockMemoryManager;
};
class MultiRootDeviceWithSubDevicesFixture : public ::testing::Test {
  public:
    void SetUp() override {
        deviceFactory = std::make_unique<UltClDeviceFactory>(3, 2);
        device1 = deviceFactory->rootDevices[1];
        device2 = deviceFactory->rootDevices[2];

        cl_device_id devices[] = {device1, device2, deviceFactory->subDevices[2]};

        context.reset(new MockContext(ClDeviceVector(devices, 3), false));
        mockMemoryManager = static_cast<MockMemoryManager *>(device1->getMemoryManager());
        ASSERT_EQ(mockMemoryManager, device1->getMemoryManager());
        for (auto &rootDeviceIndex : context->getRootDeviceIndices()) {
            mockMemoryManager->localMemorySupported[rootDeviceIndex] = true;
        }
    }

    const uint32_t expectedRootDeviceIndex = 1;
    std::unique_ptr<UltClDeviceFactory> deviceFactory;
    MockClDevice *device1 = nullptr;
    MockClDevice *device2 = nullptr;
    std::unique_ptr<MockContext> context;
    MockMemoryManager *mockMemoryManager;
};
}; // namespace NEO
