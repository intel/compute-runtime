/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

namespace NEO {
void ClDeviceFixture::setUp() {
    hardwareInfo = *defaultHwInfo;
    setUpImpl(&hardwareInfo);
}

void ClDeviceFixture::setUpImpl(const NEO::HardwareInfo *hardwareInfo) {
    pDevice = MockClDevice::createWithNewExecutionEnvironment<MockDevice>(hardwareInfo, rootDeviceIndex);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
    ASSERT_NE(nullptr, pDevice);
    pClExecutionEnvironment = static_cast<MockClExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    auto platform = NEO::constructPlatform(pClExecutionEnvironment);
    NEO::initPlatform({pDevice});
    pClDevice = static_cast<MockClDevice *>(platform->getClDevice(0u));
    ASSERT_NE(nullptr, pClDevice);

    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
    pTagMemory = commandStreamReceiver.getTagAddress();
    ASSERT_NE(nullptr, const_cast<TagAddressType *>(pTagMemory));
    this->osContext = pDevice->getDefaultEngine().osContext;
    if (pDevice->getPreemptionMode() == NEO::PreemptionMode::MidThread &&
        !commandStreamReceiver.getPreemptionAllocation()) {
        commandStreamReceiver.createPreemptionAllocation();
    }
}

void ClDeviceFixture::tearDown() {
    pClDevice = nullptr;
    pDevice = nullptr;
    NEO::cleanupPlatform(pClExecutionEnvironment);
}

MockDevice *ClDeviceFixture::createWithUsDeviceId(unsigned short usDeviceId) {
    hardwareInfo = *defaultHwInfo;
    hardwareInfo.platform.usDeviceID = usDeviceId;
    return MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, rootDeviceIndex);
}

const RootDeviceEnvironment &ClDeviceFixture::getRootDeviceEnvironment() const {
    return pClDevice->getRootDeviceEnvironment();
}

RootDeviceEnvironment &ClDeviceFixture::getMutableRootDeviceEnvironment() {
    return pClDevice->getDevice().getRootDeviceEnvironmentRef();
}

template <typename HelperType>
HelperType &ClDeviceFixture::getHelper() const {
    auto &helper = pClDevice->getRootDeviceEnvironment().getHelper<HelperType>();
    return helper;
}

template ProductHelper &ClDeviceFixture::getHelper() const;
template CompilerProductHelper &ClDeviceFixture::getHelper() const;
template GfxCoreHelper &ClDeviceFixture::getHelper() const;
template ClGfxCoreHelper &ClDeviceFixture::getHelper() const;
} // namespace NEO
