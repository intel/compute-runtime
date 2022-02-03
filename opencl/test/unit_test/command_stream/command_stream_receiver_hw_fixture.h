/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
using namespace NEO;
struct BcsTests : public Test<ClDeviceFixture> {
    void SetUp() override {
        Test<ClDeviceFixture>::SetUp();
        context = std::make_unique<MockContext>(pClDevice);
    }

    void TearDown() override {
        context.reset();
        Test<ClDeviceFixture>::TearDown();
    }

    uint32_t flushBcsTask(CommandStreamReceiver *bcsCsr, const BlitProperties &blitProperties, bool blocking, Device &device) {
        BlitPropertiesContainer container;
        container.push_back(blitProperties);

        return bcsCsr->flushBcsTask(container, blocking, false, device);
    }

    TimestampPacketContainer timestampPacketContainer;
    CsrDependencies csrDependencies;
    std::unique_ptr<MockContext> context;
};
