/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "gtest/gtest.h"

namespace NEO {
void ClDeviceFixture::SetUp() {
    hardwareInfo = *defaultHwInfo;
    SetUpImpl(&hardwareInfo);
}

void ClDeviceFixture::SetUpImpl(const NEO::HardwareInfo *hardwareInfo) {
    pDevice = MockDevice::createWithNewExecutionEnvironment<MockDevice>(hardwareInfo, rootDeviceIndex);
    ASSERT_NE(nullptr, pDevice);
    pClDevice = new MockClDevice{pDevice};
    ASSERT_NE(nullptr, pClDevice);

    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
    pTagMemory = commandStreamReceiver.getTagAddress();
    ASSERT_NE(nullptr, const_cast<uint32_t *>(pTagMemory));
}

void ClDeviceFixture::TearDown() {
    delete pClDevice;
    pClDevice = nullptr;
    pDevice = nullptr;
}

MockDevice *ClDeviceFixture::createWithUsDeviceId(unsigned short usDeviceId) {
    hardwareInfo = *defaultHwInfo;
    hardwareInfo.platform.usDeviceID = usDeviceId;
    return MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, rootDeviceIndex);
}
} // namespace NEO
