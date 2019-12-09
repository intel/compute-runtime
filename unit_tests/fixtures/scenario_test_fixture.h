/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/options.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "gtest/gtest.h"

using namespace NEO;

class ScenarioTest : public ::testing::Test,
                     public PlatformFixture {
    using PlatformFixture::SetUp;

  protected:
    void SetUp() override {
        DebugManager.flags.EnableTimestampPacket.set(false);
        PlatformFixture::SetUp();

        auto pDevice = pPlatform->getDevice(0);
        ASSERT_NE(nullptr, pDevice);

        cl_device_id clDevice = pDevice;
        context = Context::create<MockContext>(nullptr, DeviceVector(&clDevice, 1), nullptr, nullptr, retVal);
        commandQueue = new MockCommandQueue(context, pDevice, 0);
        program = new MockProgram(*pDevice->getExecutionEnvironment(), context, false);

        kernelInternals = new MockKernelWithInternals(*pDevice, context);
        kernel = kernelInternals->mockKernel;

        ASSERT_NE(nullptr, kernel);
    }
    void TearDown() override {
        delete kernelInternals;
        delete commandQueue;
        context->release();
        program->release();

        PlatformFixture::TearDown();
    }

    cl_int retVal;
    DebugManagerStateRestore dbgRestorer;
    MockCommandQueue *commandQueue = nullptr;
    MockContext *context = nullptr;
    MockKernelWithInternals *kernelInternals = nullptr;
    MockKernel *kernel = nullptr;
    MockProgram *program = nullptr;
};
