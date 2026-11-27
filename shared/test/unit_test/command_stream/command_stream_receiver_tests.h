/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/fixtures/command_stream_receiver_fixture.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

struct CommandStreamReceiverTest : public NEO::DeviceFixture,
                                   public ::testing::Test {
    void SetUp() override {
        DeviceFixture::setUp();

        commandStreamReceiver = &pDevice->getGpgpuCommandStreamReceiver();
        ASSERT_NE(nullptr, commandStreamReceiver);
        memoryManager = commandStreamReceiver->getMemoryManager();
        internalAllocationStorage = commandStreamReceiver->getInternalAllocationStorage();

        auto &compilerProductHelper = pDevice->getCompilerProductHelper();
        auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*NEO::defaultHwInfo);
        this->heaplessModeEnabled = heaplessEnabled;
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }

    NEO::CommandStreamReceiver *commandStreamReceiver = nullptr;
    NEO::MemoryManager *memoryManager = nullptr;
    NEO::InternalAllocationStorage *internalAllocationStorage = nullptr;
    bool heaplessModeEnabled = false;
};

using CommandStreamReceiverHwTest = Test<CommandStreamReceiverFixture>;
