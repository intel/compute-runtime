/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gtest/gtest.h"
#include "unit_tests/fixtures/device_fixture.h"

namespace OCLRT {
void DeviceFixture::SetUp() {
    SetUpImpl(nullptr);
}

void DeviceFixture::SetUpImpl(const OCLRT::HardwareInfo *hardwareInfo) {
    pDevice = MockDevice::createWithNewExecutionEnvironment<MockDevice>(hardwareInfo);
    ASSERT_NE(nullptr, pDevice);

    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    pTagMemory = commandStreamReceiver.getTagAddress();
    ASSERT_NE(nullptr, const_cast<uint32_t *>(pTagMemory));
}

void DeviceFixture::TearDown() {
    delete pDevice;
}

MockDevice *DeviceFixture::createWithUsDeviceId(unsigned short usDeviceId) {
    hwInfoHelper = *platformDevices[0];
    platformHelper = *platformDevices[0]->pPlatform;
    platformHelper.usDeviceID = usDeviceId;
    hwInfoHelper.pPlatform = &platformHelper;
    return MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfoHelper);
}
} // namespace OCLRT