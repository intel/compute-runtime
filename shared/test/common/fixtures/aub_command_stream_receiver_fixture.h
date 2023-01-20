/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/mocks/mock_device.h"

namespace NEO {
struct AubCommandStreamReceiverFixture : public DeviceFixture, MockAubCenterFixture {
    void setUp() {
        DeviceFixture::setUp();
        MockAubCenterFixture::setUp();
        setMockAubCenter(*pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    }
    void tearDown() {
        MockAubCenterFixture::tearDown();
        DeviceFixture::tearDown();
    }
};
} // namespace NEO
