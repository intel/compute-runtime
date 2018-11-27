/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "runtime/helpers/options.h"

#include "gtest/gtest.h"
#include "test.h"

using namespace OCLRT;

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
