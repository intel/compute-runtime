/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "test.h"
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

    uint32_t blitBuffer(CommandStreamReceiver *bcsCsr, const BlitProperties &blitProperties, bool blocking, Device &device) {
        BlitPropertiesContainer container;
        container.push_back(blitProperties);

        return bcsCsr->blitBuffer(container, blocking, false, device);
    }

    TimestampPacketContainer timestampPacketContainer;
    CsrDependencies csrDependencies;
    std::unique_ptr<MockContext> context;
};
