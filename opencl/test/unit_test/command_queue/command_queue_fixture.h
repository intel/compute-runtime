/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "CL/cl.h"

namespace NEO {
class Device;

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

    virtual void SetUp();                                                          // NOLINT(readability-identifier-naming)
    virtual void SetUp(ClDevice *pDevice, cl_command_queue_properties properties); // NOLINT(readability-identifier-naming)

    virtual void TearDown(); // NOLINT(readability-identifier-naming)

    CommandQueue *pCmdQ = nullptr;
    MockClDevice *device = nullptr;
    MockContext *context = nullptr;
    bool createdDevice = false;
};

struct OOQueueFixture : public CommandQueueHwFixture {
    typedef CommandQueueHwFixture BaseClass;

    void SetUp(ClDevice *pDevice, cl_command_queue_properties properties) override;
};

struct CommandQueueFixture {
    virtual void SetUp( // NOLINT(readability-identifier-naming)
        Context *context,
        ClDevice *device,
        cl_command_queue_properties properties);
    virtual void TearDown(); // NOLINT(readability-identifier-naming)

    CommandQueue *createCommandQueue(
        Context *context,
        ClDevice *device,
        cl_command_queue_properties properties,
        bool internalUsage);

    CommandQueue *pCmdQ = nullptr;
};

static const cl_command_queue_properties AllCommandQueueProperties[] = {
    0,
    CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
    CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
    CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE_DEFAULT,
    CL_QUEUE_PROFILING_ENABLE,
    CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
    CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE_DEFAULT};

static const cl_command_queue_properties DefaultCommandQueueProperties[] = {
    0,
    CL_QUEUE_PROFILING_ENABLE,
};

template <bool ooq>
struct CommandQueueHwBlitTest : ClDeviceFixture, ContextFixture, CommandQueueHwFixture, ::testing::Test {
    using ContextFixture::SetUp;

    void SetUp() override {
        hwInfo = *::defaultHwInfo;
        hwInfo.capabilityTable.blitterOperationsSupported = true;
        REQUIRE_FULL_BLITTER_OR_SKIP(&hwInfo);

        DebugManager.flags.EnableBlitterOperationsSupport.set(1);
        DebugManager.flags.EnableTimestampPacket.set(1);
        DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.set(1);
        ClDeviceFixture::setUpImpl(&hwInfo);
        cl_device_id device = pClDevice;
        ContextFixture::SetUp(1, &device);
        cl_command_queue_properties queueProperties = ooq ? CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE : 0;
        CommandQueueHwFixture::SetUp(pClDevice, queueProperties);
    }

    void TearDown() override {
        CommandQueueHwFixture::TearDown();
        ContextFixture::TearDown();
        ClDeviceFixture::TearDown();
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

    using ContextFixture::SetUp;

    void SetUp() override;

    void TearDown() override;

    cl_command_queue_properties properties;
    const HardwareInfo *pHwInfo = nullptr;
};

struct OOQueueHwTest : public ClDeviceFixture,
                       public ContextFixture,
                       public OOQueueFixture,
                       ::testing::Test {
    using ContextFixture::SetUp;

    OOQueueHwTest() {
    }

    void SetUp() override;

    void SetUp(ClDevice *pDevice, cl_command_queue_properties properties) override {
    }

    void TearDown() override;
};

} // namespace NEO
