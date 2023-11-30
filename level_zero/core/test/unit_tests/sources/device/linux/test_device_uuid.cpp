/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

struct TestDeviceUuid : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
    DebugManagerStateRestore restorer;
};

TEST_F(TestDeviceUuid, GivenEnableChipsetUniqueUuidIsSetWhenOsInterfaceIsNotSetThenUuidOfFallbackPathIsReceived) {

    debugManager.flags.EnableChipsetUniqueUUID.set(1);
    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    auto device = driverHandle->devices[0];

    ze_device_properties_t deviceProperties, devicePropertiesBefore;
    deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    memset(&deviceProperties.uuid, std::numeric_limits<int>::max(), sizeof(deviceProperties.uuid));
    devicePropertiesBefore = deviceProperties;
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getProperties(&deviceProperties));
    EXPECT_NE(0, memcmp(&deviceProperties.uuid, &devicePropertiesBefore.uuid, sizeof(devicePropertiesBefore.uuid)));
}

} // namespace ult
} // namespace L0
