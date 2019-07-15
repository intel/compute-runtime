/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/device_fixture.h"

#include "gtest/gtest.h"

namespace NEO {
void DeviceFixture::SetUp() {
    hardwareInfo = *platformDevices[0];
    SetUpImpl(&hardwareInfo);
}

void DeviceFixture::SetUpImpl(const NEO::HardwareInfo *hardwareInfo) {
    pDevice = MockDevice::createWithNewExecutionEnvironment<MockDevice>(hardwareInfo);
    ASSERT_NE(nullptr, pDevice);

    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
    pTagMemory = commandStreamReceiver.getTagAddress();
    ASSERT_NE(nullptr, const_cast<uint32_t *>(pTagMemory));
}

void DeviceFixture::TearDown() {
    delete pDevice;
    pDevice = nullptr;
}

MockDevice *DeviceFixture::createWithUsDeviceId(unsigned short usDeviceId) {
    hardwareInfo = *platformDevices[0];
    hardwareInfo.platform.usDeviceID = usDeviceId;
    return MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo);
}
} // namespace NEO
