/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

class EnqueueHandlerTest : public NEO::ClDeviceFixture,
                           public testing::Test {
  public:
    void SetUp() override {
        ClDeviceFixture::setUp();
        context = new NEO::MockContext(pClDevice);
    }

    void TearDown() override {
        context->decRefInternal();
        ClDeviceFixture::tearDown();
    }
    NEO::MockContext *context;
};
