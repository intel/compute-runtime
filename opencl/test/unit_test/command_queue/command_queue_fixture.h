/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

namespace NEO {
class Device;
class Buffer;
class ClDevice;
class CommandQueue;
class Context;
class MockContext;

struct CommandQueueHwFixture {
    CommandQueue *createCommandQueue(ClDevice *device) {
        return createCommandQueue(device, cl_command_queue_properties{0});
    }

    CommandQueue *createCommandQueue(
        ClDevice *device,
        cl_command_queue_properties properties);

    CommandQueue *createCommandQueue(
        ClDevice *device,
        const cl_command_queue_properties *properties);

    CommandQueue *createCommandQueue(
        ClDevice *device,
        const cl_command_queue_properties *properties,
        Context *context);

    static void forceMapBufferOnGpu(Buffer &buffer);

    void setUp();
    void setUp(ClDevice *pDevice, cl_command_queue_properties properties);

    void tearDown();

    CommandQueue *pCmdQ = nullptr;
    MockClDevice *device = nullptr;
    MockContext *context = nullptr;
    bool createdDevice = false;
};

struct OOQueueFixture : public CommandQueueHwFixture {
    typedef CommandQueueHwFixture BaseClass;

    void setUp(ClDevice *pDevice, cl_command_queue_properties properties);
};

struct CommandQueueFixture {
    void setUp(
        Context *context,
        ClDevice *device,
        cl_command_queue_properties properties);
    void tearDown();

    CommandQueue *createCommandQueue(
        Context *context,
        ClDevice *device,
        cl_command_queue_properties properties,
        bool internalUsage);

    CommandQueue *pCmdQ = nullptr;
};

static const cl_command_queue_properties allCommandQueueProperties[] = {
    0,
    CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
    CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
    CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE_DEFAULT,
    CL_QUEUE_PROFILING_ENABLE,
    CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
    CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE_DEFAULT};

static const cl_command_queue_properties defaultCommandQueueProperties[] = {
    0,
    CL_QUEUE_PROFILING_ENABLE,
};

template <bool ooq>
struct CommandQueueHwBlitTest : ClDeviceFixture, ContextFixture, CommandQueueHwFixture, ::testing::Test {
    using ContextFixture::setUp;

    void SetUp() override {
        hwInfo = *::defaultHwInfo;
        hwInfo.capabilityTable.blitterOperationsSupported = true;

        debugManager.flags.EnableBlitterOperationsSupport.set(1);
        debugManager.flags.EnableTimestampPacket.set(1);
        debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
        debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
        ClDeviceFixture::setUpImpl(&hwInfo);
        cl_device_id device = pClDevice;
        REQUIRE_FULL_BLITTER_OR_SKIP(pClDevice->getRootDeviceEnvironment());

        ContextFixture::setUp(1, &device);
        cl_command_queue_properties queueProperties = ooq ? CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE : 0;
        CommandQueueHwFixture::setUp(pClDevice, queueProperties);
    }

    void TearDown() override {
        CommandQueueHwFixture::tearDown();
        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    HardwareInfo hwInfo{};
    DebugManagerStateRestore state{};
};

using IoqCommandQueueHwBlitTest = CommandQueueHwBlitTest<false>;
using OoqCommandQueueHwBlitTest = CommandQueueHwBlitTest<true>;

struct CommandQueueHwTest
    : public ClDeviceFixture,
      public ContextFixture,
      public CommandQueueHwFixture,
      ::testing::Test {

    using ContextFixture::setUp;

    void SetUp() override;

    void TearDown() override;

    cl_command_queue_properties properties;
    const HardwareInfo *pHwInfo = nullptr;
};

template <template <typename> class CsrType>
struct CommandQueueHwTestWithCsrT
    : public CommandQueueHwTest {

    void SetUp() override {}

    void TearDown() override {}

    template <typename FamilyType>
    void setUpT() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<CsrType<FamilyType>>();
        CommandQueueHwTest::SetUp();
    }

    template <typename FamilyType>
    void tearDownT() {
        CommandQueueHwTest::TearDown();
    }
};

struct OOQueueHwTest : public ClDeviceFixture,
                       public ContextFixture,
                       public OOQueueFixture,
                       ::testing::Test {
    using ContextFixture::setUp;

    OOQueueHwTest() {
    }

    void SetUp() override;

    void setUp(ClDevice *pDevice, cl_command_queue_properties properties) {
    }

    void TearDown() override;
};

} // namespace NEO
