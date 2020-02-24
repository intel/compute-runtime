/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/aub_memory_operations_handler.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

using namespace NEO;

class AubMemoryOperationsHandlerTests : public ::testing::Test {
  public:
    void SetUp() override {
        DebugManager.flags.SetCommandStreamReceiver.set(2);
        residencyHandler = std::unique_ptr<AubMemoryOperationsHandler>(new AubMemoryOperationsHandler(nullptr));

        allocPtr = &allocation;
    }

    AubMemoryOperationsHandler *getMemoryOperationsHandler() {
        return residencyHandler.get();
    }

    MockGraphicsAllocation allocation;
    GraphicsAllocation *allocPtr;
    DebugManagerStateRestore dbgRestore;
    std::unique_ptr<AubMemoryOperationsHandler> residencyHandler;
};
