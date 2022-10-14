/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/sources/device/test_device_pci_speed_info.h"

#include "shared/source/os_interface/driver_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
namespace L0 {
namespace ult {

std::unique_ptr<NEO::UltDeviceFactory> PciSpeedInfoTest::createDevices(uint32_t numSubDevices, const NEO::PhysicalDevicePciSpeedInfo &pciSpeedInfo) {

    DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
    DebugManager.flags.EnableChipsetUniqueUUID.set(0);
    NEO::ExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, 1);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OSInterface);
    executionEnvironment->memoryManager.reset(new MockMemoryManagerOsAgnosticContext(*executionEnvironment));
    setPciSpeedInfo(executionEnvironment, pciSpeedInfo);
    return std::make_unique<UltDeviceFactory>(1, numSubDevices, *executionEnvironment);
}

TEST_F(PciSpeedInfoTest, givenSuccessfulReadingOfSpeedValuesCorrectValuesAreReturned) {

    NEO::PhysicalDevicePciSpeedInfo expectedSpeedInfo;
    expectedSpeedInfo.genVersion = 4;
    expectedSpeedInfo.width = 1024;
    expectedSpeedInfo.maxBandwidth = 4096;

    auto deviceFactory = createDevices(2, expectedSpeedInfo);
    auto driverHandle = std::make_unique<DriverHandleImp>();
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), deviceFactory->rootDevices[0], false, &returnValue));

    ze_pci_ext_properties_t pciProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getPciProperties(&pciProperties));
    EXPECT_EQ(4, pciProperties.maxSpeed.genVersion);
    EXPECT_EQ(1024, pciProperties.maxSpeed.width);
    EXPECT_EQ(4096, pciProperties.maxSpeed.maxBandwidth);

    uint32_t subDeviceCount = 0;
    device->getSubDevices(&subDeviceCount, nullptr);
    EXPECT_EQ(subDeviceCount, 2u);
    std::vector<ze_device_handle_t> subDevices;
    subDevices.resize(subDeviceCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getSubDevices(&subDeviceCount, subDevices.data()));
    for (auto subDevice : subDevices) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeDevicePciGetPropertiesExt(subDevice, &pciProperties));
        EXPECT_EQ(4, pciProperties.maxSpeed.genVersion);
        EXPECT_EQ(1024, pciProperties.maxSpeed.width);
        EXPECT_EQ(4096, pciProperties.maxSpeed.maxBandwidth);
    }
}

} // namespace ult
} // namespace L0
