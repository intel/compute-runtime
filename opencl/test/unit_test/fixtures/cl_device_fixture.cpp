/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "shared/source/built_ins/sip.h"

#include "opencl/source/helpers/cl_hw_helper.h"

#include "gtest/gtest.h"

namespace NEO {
void ClDeviceFixture::setUp() {
    hardwareInfo = *defaultHwInfo;
    setUpImpl(&hardwareInfo);
}

void ClDeviceFixture::setUpImpl(const NEO::HardwareInfo *hardwareInfo) {
    pDevice = MockClDevice::createWithNewExecutionEnvironment<MockDevice>(hardwareInfo, rootDeviceIndex);
    ASSERT_NE(nullptr, pDevice);
    pClExecutionEnvironment = static_cast<MockClExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    pClDevice = new MockClDevice{pDevice};
    ASSERT_NE(nullptr, pClDevice);

    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
    pTagMemory = commandStreamReceiver.getTagAddress();
    ASSERT_NE(nullptr, const_cast<TagAddressType *>(pTagMemory));
    this->osContext = pDevice->getDefaultEngine().osContext;
}

void ClDeviceFixture::tearDown() {
    delete pClDevice;
    pClDevice = nullptr;
    pDevice = nullptr;
}

MockDevice *ClDeviceFixture::createWithUsDeviceId(unsigned short usDeviceId) {
    hardwareInfo = *defaultHwInfo;
    hardwareInfo.platform.usDeviceID = usDeviceId;
    return MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, rootDeviceIndex);
}

const RootDeviceEnvironment &ClDeviceFixture::getRootDeviceEnvironment() const {
    return pClDevice->getRootDeviceEnvironment();
}

template <typename HelperType>
HelperType &ClDeviceFixture::getHelper() const {
    auto &helper = pClDevice->getRootDeviceEnvironment().getHelper<HelperType>();
    return helper;
}

template ProductHelper &ClDeviceFixture::getHelper() const;
template GfxCoreHelper &ClDeviceFixture::getHelper() const;
template ClGfxCoreHelper &ClDeviceFixture::getHelper() const;
} // namespace NEO
