/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/device_fixture.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/execution_environment/root_device_environment.h"

#include "gtest/gtest.h"

namespace NEO {
void DeviceFixture::setUp() {
    hardwareInfo = *defaultHwInfo;
    setUpImpl(&hardwareInfo);
}

void DeviceFixture::setUpImpl(const NEO::HardwareInfo *hardwareInfo) {
    pDevice = MockDevice::createWithNewExecutionEnvironment<MockDevice>(hardwareInfo, rootDeviceIndex);
    ASSERT_NE(nullptr, pDevice);

    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
    pTagMemory = commandStreamReceiver.getTagAddress();
    ASSERT_NE(nullptr, const_cast<TagAddressType *>(pTagMemory));
}

void DeviceFixture::tearDown() {
    delete pDevice;
    pDevice = nullptr;
}

MockDevice *DeviceFixture::createWithUsDeviceIdRevId(unsigned short usDeviceId, unsigned short usRevId) {
    hardwareInfo = *defaultHwInfo;
    hardwareInfo.platform.usDeviceID = usDeviceId;
    hardwareInfo.platform.usRevId = usRevId;
    return MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo);
}

template <typename HelperType>
HelperType &DeviceFixture::getHelper() const {
    auto &helper = this->pDevice->getRootDeviceEnvironment().getHelper<HelperType>();
    return helper;
}

template ProductHelper &DeviceFixture::getHelper<ProductHelper>() const;
template GfxCoreHelper &DeviceFixture::getHelper<GfxCoreHelper>() const;

} // namespace NEO
