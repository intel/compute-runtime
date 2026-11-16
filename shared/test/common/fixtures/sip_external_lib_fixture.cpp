/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/sip_external_lib_fixture.h"

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/mocks/mock_sip_external_lib.h"

namespace NEO {
void SipExternalLibFixture::injectSipLib(Device &device) {
    auto mockSipLib = std::make_unique<MockSipExternalLib>();
    this->sipExternalLib = mockSipLib.get();
    device.getRootDeviceEnvironmentRef().sipExternalLib = std::move(mockSipLib);
}
} // namespace NEO
