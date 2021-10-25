/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "shared/source/built_ins/sip.h"

#include "gtest/gtest.h"

namespace NEO {
void ClDeviceFixture::SetUp() {
    hardwareInfo = *defaultHwInfo;
    SetUpImpl(&hardwareInfo);
}

void ClDeviceFixture::SetUpImpl(const NEO::HardwareInfo *hardwareInfo) {
    pDevice = MockClDevice::createWithNewExecutionEnvironment<MockDevice>(hardwareInfo, rootDeviceIndex);
    ASSERT_NE(nullptr, pDevice);
    pClExecutionEnvironment = static_cast<MockClExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    pClDevice = new MockClDevice{pDevice};
    ASSERT_NE(nullptr, pClDevice);

    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
    pTagMemory = commandStreamReceiver.getTagAddress();
    ASSERT_NE(nullptr, const_cast<uint32_t *>(pTagMemory));
    this->osContext = pDevice->getDefaultEngine().osContext;
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
