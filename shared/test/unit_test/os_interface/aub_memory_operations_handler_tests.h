/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/aub_memory_operations_handler.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_aub_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

using namespace NEO;

class AubMemoryOperationsHandlerTests : public ::testing::Test {
  public:
    void SetUp() override {
        debugManager.flags.SetCommandStreamReceiver.set(2);
        residencyHandler = std::unique_ptr<MockAubMemoryOperationsHandler>(new MockAubMemoryOperationsHandler(nullptr));

        hardwareInfo = *defaultHwInfo;

        device.reset(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hardwareInfo, 0));

        allocPtr = &allocation;
    }

    MockAubMemoryOperationsHandler *getMemoryOperationsHandler() {
        return residencyHandler.get();
    }

    MockGraphicsAllocation allocation{reinterpret_cast<void *>(0x1000), 0x1000u, MemoryConstants::pageSize};
    GraphicsAllocation *allocPtr;
    DebugManagerStateRestore dbgRestore;
    HardwareInfo hardwareInfo = {};
    std::unique_ptr<MockAubMemoryOperationsHandler> residencyHandler;
    std::unique_ptr<NEO::MockDevice> device;
};
