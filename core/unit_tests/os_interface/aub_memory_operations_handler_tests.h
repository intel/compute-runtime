/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/hw_info.h"
#include "core/helpers/options.h"
#include "core/os_interface/aub_memory_operations_handler.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/os_interface/device_factory.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

using namespace NEO;

class AubMemoryOperationsHandlerTests : public ::testing::Test {
  public:
    void SetUp() override {
        DebugManager.flags.SetCommandStreamReceiver.set(2);
        residencyHandler = std::unique_ptr<AubMemoryOperationsHandler>(new AubMemoryOperationsHandler(nullptr));
    }

    AubMemoryOperationsHandler *getMemoryOperationsHandler() {
        return residencyHandler.get();
    }

    MockGraphicsAllocation allocation;
    DebugManagerStateRestore dbgRestore;
    std::unique_ptr<AubMemoryOperationsHandler> residencyHandler;
};
