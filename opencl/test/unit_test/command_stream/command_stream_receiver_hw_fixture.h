/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/timestamp_packet_container.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
using namespace NEO;
struct BcsTests : public Test<ClDeviceFixture> {
    void SetUp() override {
        debugManager.flags.ForceDummyBlitWa.set(-1);
        Test<ClDeviceFixture>::SetUp();
        context = std::make_unique<MockContext>(pClDevice);
    }

    void TearDown() override {
        context.reset();
        Test<ClDeviceFixture>::TearDown();
    }

    TaskCountType flushBcsTask(CommandStreamReceiver *bcsCsr, const BlitProperties &blitProperties, bool blocking, Device &device) {
        BlitPropertiesContainer container;
        container.push_back(blitProperties);

        return bcsCsr->flushBcsTask(container, blocking, false, device);
    }

    TimestampPacketContainer timestampPacketContainer;
    CsrDependencies csrDependencies;
    std::unique_ptr<MockContext> context;
    DebugManagerStateRestore dbgRestore;
};
