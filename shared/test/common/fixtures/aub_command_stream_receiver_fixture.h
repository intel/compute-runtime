/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/unit_test/fixtures/mock_aub_center_fixture.h"

namespace NEO {
struct AubCommandStreamReceiverFixture : public DeviceFixture, MockAubCenterFixture {
    void SetUp() {
        DeviceFixture::SetUp();
        MockAubCenterFixture::SetUp();
        setMockAubCenter(*pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    }
    void TearDown() {
        MockAubCenterFixture::TearDown();
        DeviceFixture::TearDown();
    }
};
} // namespace NEO
