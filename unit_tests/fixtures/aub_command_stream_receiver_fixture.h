/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/mock_aub_center_fixture.h"

namespace NEO {
struct AubCommandStreamReceiverFixture : public DeviceFixture, MockAubCenterFixture {
    void SetUp() {
        DeviceFixture::SetUp();
        MockAubCenterFixture::SetUp();
        setMockAubCenter(pDevice->getExecutionEnvironment());
    }
    void TearDown() {
        MockAubCenterFixture::TearDown();
        DeviceFixture::TearDown();
    }
};
} // namespace NEO
