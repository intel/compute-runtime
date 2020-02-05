/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/hw_info.h"
#include "core/os_interface/aub_memory_operations_handler.h"
#include "core/os_interface/device_factory.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

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
