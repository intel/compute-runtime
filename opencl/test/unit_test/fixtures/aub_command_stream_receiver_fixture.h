/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/unit_test/fixtures/mock_aub_center_fixture.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

namespace NEO {
struct AubCommandStreamReceiverFixture : public ClDeviceFixture, MockAubCenterFixture {
    void SetUp() {
        ClDeviceFixture::SetUp();
        MockAubCenterFixture::SetUp();
        setMockAubCenter(*pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    }
    void TearDown() {
        MockAubCenterFixture::TearDown();
        ClDeviceFixture::TearDown();
    }
};
} // namespace NEO
