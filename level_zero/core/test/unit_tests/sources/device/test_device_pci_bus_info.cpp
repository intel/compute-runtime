/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/sources/device/test_device_pci_bus_info.h"

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/fabric/fabric.h"

namespace L0 {
namespace ult {

TEST_F(PciBusInfoTest, givenSuccessfulReadingOfBusValuesThenCorrectValuesAreReturned) {
    constexpr uint32_t numRootDevices = 1u;
    constexpr uint32_t numSubDevices = 2u;
    const NEO::PhysicalDevicePciBusInfo expectedBusInfo{1u, 3u, 5u, 7u};
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
    debugManager.flags.EnableChipsetUniqueUUID.set(0);
    NEO::ExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, numRootDevices);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OSInterface);
    executionEnvironment->memoryManager.reset(new MockMemoryManagerOsAgnosticContext(*executionEnvironment));
    setPciBusInfo(executionEnvironment, expectedBusInfo, 0);
    auto deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);

    auto driverHandle = std::make_unique<DriverHandleImp>();
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), deviceFactory->rootDevices[0], false, &returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_pci_ext_properties_t pciProperties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getPciProperties(&pciProperties));
    EXPECT_EQ(expectedBusInfo.pciDomain, pciProperties.address.domain);
    EXPECT_EQ(expectedBusInfo.pciBus, pciProperties.address.bus);
    EXPECT_EQ(expectedBusInfo.pciDevice, pciProperties.address.device);
    EXPECT_EQ(expectedBusInfo.pciFunction, pciProperties.address.function);

    uint32_t subDeviceCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getSubDevices(&subDeviceCount, nullptr));
    EXPECT_EQ(subDeviceCount, numSubDevices);
    std::vector<ze_device_handle_t> subDevices;
    subDevices.resize(subDeviceCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getSubDevices(&subDeviceCount, subDevices.data()));
    for (auto subDevice : subDevices) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeDevicePciGetPropertiesExt(subDevice, &pciProperties));
        EXPECT_EQ(expectedBusInfo.pciDomain, pciProperties.address.domain);
        EXPECT_EQ(expectedBusInfo.pciBus, pciProperties.address.bus);
        EXPECT_EQ(expectedBusInfo.pciDevice, pciProperties.address.device);
        EXPECT_EQ(expectedBusInfo.pciFunction, pciProperties.address.function);
    }
}

TEST_P(PciBusOrderingTest, givenMultipleDevicesAndZePcieIdOrderingSetThenDevicesAndVerticesAreInCorrectOrder) {
    constexpr uint32_t numRootDevices = 6u;
    constexpr uint32_t numSubDevices = 2u;
    const NEO::PhysicalDevicePciBusInfo busInfos[numRootDevices] = {{2, 0, 3, 0}, {2, 0, 1, 9}, {0, 0, 0, 1}, {0, 3, 5, 0}, {1, 3, 5, 0}, {0, 4, 1, 0}};
    const NEO::PhysicalDevicePciBusInfo busInfosSorted[numRootDevices] = {{0, 3, 5, 0}, {0, 4, 1, 0}, {2, 0, 1, 9}, {0, 0, 0, 1}, {1, 3, 5, 0}, {2, 0, 3, 0}};
    const NEO::PhysicalDevicePciBusInfo busInfosPciSorted[numRootDevices] = {{0, 0, 0, 1}, {0, 3, 5, 0}, {0, 4, 1, 0}, {1, 3, 5, 0}, {2, 0, 1, 9}, {2, 0, 3, 0}};
    const bool forcePciBusOrdering = GetParam();

    debugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(forcePciBusOrdering ? 1 : 0);
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
    debugManager.flags.EnableChipsetUniqueUUID.set(0);

    auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    auto memoryManager = new MockMemoryManagerOsAgnosticContext(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto neoDevices = NEO::DeviceFactory::createDevices(*executionEnvironment);
    EXPECT_EQ(numRootDevices, neoDevices.size());
    EXPECT_EQ(numRootDevices, executionEnvironment->rootDeviceEnvironments.size());
    for (auto i = 0u; i < numRootDevices; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset(new OSInterface);
        setPciBusInfo(executionEnvironment, busInfos[i], i);
        if (i % 2 == 0) {
            executionEnvironment->rootDeviceEnvironments[i]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
        } else {
            executionEnvironment->rootDeviceEnvironments[i]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
        }
    }
    executionEnvironment->sortNeoDevices();

    auto deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
    EXPECT_EQ(numRootDevices, deviceFactory->rootDevices.size());

    auto driverHandle = std::make_unique<DriverHandleImp>();
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(neoDevices)));
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::vector<std::unique_ptr<L0::Device>> devices;
    for (auto i = 0u; i < numRootDevices; i++) {
        devices.emplace_back(L0::Device::create(driverHandle.get(), deviceFactory->rootDevices[i], false, &returnValue));
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    }
    EXPECT_EQ(0u, driverHandle->fabricVertices.size());
    uint32_t vertexCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricVertexGetExp(&vertexCount, nullptr));
    EXPECT_EQ(numRootDevices, vertexCount);
    EXPECT_EQ(numRootDevices, driverHandle->fabricVertices.size());
    std::vector<ze_fabric_vertex_handle_t> vertices(vertexCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricVertexGetExp(&vertexCount, vertices.data()));

    for (auto i = 0u; i < numRootDevices; i++) {
        ze_pci_ext_properties_t pciProperties{};
        EXPECT_EQ(ZE_RESULT_SUCCESS, devices[i]->getPciProperties(&pciProperties));
        NEO::PhysicalDevicePciBusInfo expectedBusInfo{};
        if (forcePciBusOrdering) {
            expectedBusInfo = busInfosPciSorted[i];
        } else {
            expectedBusInfo = busInfosSorted[i];
        }
        EXPECT_EQ(expectedBusInfo.pciDomain, pciProperties.address.domain);
        EXPECT_EQ(expectedBusInfo.pciBus, pciProperties.address.bus);
        EXPECT_EQ(expectedBusInfo.pciDevice, pciProperties.address.device);
        EXPECT_EQ(expectedBusInfo.pciFunction, pciProperties.address.function);

        uint32_t subDeviceCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, devices[i]->getSubDevices(&subDeviceCount, nullptr));
        EXPECT_EQ(subDeviceCount, numSubDevices);
        std::vector<ze_device_handle_t> subDevices;
        subDevices.resize(subDeviceCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, devices[i]->getSubDevices(&subDeviceCount, subDevices.data()));
        for (auto subDevice : subDevices) {
            ze_pci_ext_properties_t pciPropertiesSubDevice{};
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeDevicePciGetPropertiesExt(subDevice, &pciPropertiesSubDevice));
            EXPECT_EQ(expectedBusInfo.pciDomain, pciPropertiesSubDevice.address.domain);
            EXPECT_EQ(expectedBusInfo.pciBus, pciPropertiesSubDevice.address.bus);
            EXPECT_EQ(expectedBusInfo.pciDevice, pciPropertiesSubDevice.address.device);
            EXPECT_EQ(expectedBusInfo.pciFunction, pciPropertiesSubDevice.address.function);
        }

        ze_fabric_vertex_exp_properties_t fabricProp{};
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::FabricVertex::fromHandle(vertices[i])->getProperties(&fabricProp));
        EXPECT_EQ(expectedBusInfo.pciDomain, fabricProp.address.domain);
        EXPECT_EQ(expectedBusInfo.pciBus, fabricProp.address.bus);
        EXPECT_EQ(expectedBusInfo.pciDevice, fabricProp.address.device);
        EXPECT_EQ(expectedBusInfo.pciFunction, fabricProp.address.function);
    }
}

INSTANTIATE_TEST_SUITE_P(, PciBusOrderingTest, ::testing::Bool());

} // namespace ult
} // namespace L0
