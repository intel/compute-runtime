/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using namespace NEO;

template <typename GfxFamily>
struct CommandStreamReceiverHwTest : public ClDeviceFixture,
                                     public HardwareParse,
                                     public ::testing::Test {

    void SetUp() override {
        environment.setCsrType<MockCsrHw<GfxFamily>>();
        ClDeviceFixture::setUp();
        HardwareParse::setUp();
    }

    void TearDown() override {
        HardwareParse::tearDown();
        ClDeviceFixture::tearDown();
    }

    EnvironmentWithCsrWrapper environment;
};
