/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"

namespace NEO {
class MultiRootDeviceFixture : public ::testing::Test {
  public:
    void SetUp() override {
        DebugManager.flags.CreateMultipleRootDevices.set(2 * expectedRootDeviceIndex);
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, expectedRootDeviceIndex));
        context.reset(new MockContext(device.get()));
        mockMemoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    }

    const uint32_t expectedRootDeviceIndex = 1;
    DebugManagerStateRestore restorer;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    MockMemoryManager *mockMemoryManager;
};
}; // namespace NEO
