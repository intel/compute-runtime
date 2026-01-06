/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/device_fixture.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/mocks/mock_device.h"

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

const ReleaseHelper *DeviceFixture::getReleaseHelper() {
    const auto *releaseHelper = this->pDevice->getRootDeviceEnvironment().getReleaseHelper();
    return releaseHelper;
}

template ProductHelper &DeviceFixture::getHelper<ProductHelper>() const;
template GfxCoreHelper &DeviceFixture::getHelper<GfxCoreHelper>() const;
template CompilerProductHelper &DeviceFixture::getHelper<CompilerProductHelper>() const;

void DeviceWithoutSipFixture::setUp() {
    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(NEO::PreemptionMode::Disabled));
    DeviceFixture::setUp();

    for (auto &rootDeviceEnvironment : pDevice->executionEnvironment->rootDeviceEnvironments) {
        for (auto &sipKernel : rootDeviceEnvironment->sipKernels) {
            EXPECT_EQ(nullptr, sipKernel);
        }
    }
    debugManager.flags.ForcePreemptionMode.set(dbgRestorer.debugVarSnapshot.ForcePreemptionMode.get());
}

void DeviceWithoutSipFixture::tearDown() {
    DeviceFixture::tearDown();
}

} // namespace NEO
