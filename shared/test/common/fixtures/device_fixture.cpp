/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/device_fixture.h"

#include "shared/source/built_ins/sip.h"

#include "gtest/gtest.h"

namespace NEO {
void DeviceFixture::SetUp() {
    hardwareInfo = *defaultHwInfo;
    SetUpImpl(&hardwareInfo);
}

void DeviceFixture::SetUpImpl(const NEO::HardwareInfo *hardwareInfo) {
    pDevice = MockDevice::createWithNewExecutionEnvironment<MockDevice>(hardwareInfo, rootDeviceIndex);
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
    hardwareInfo = *defaultHwInfo;
    hardwareInfo.platform.usDeviceID = usDeviceId;
    return MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo);
}
} // namespace NEO
