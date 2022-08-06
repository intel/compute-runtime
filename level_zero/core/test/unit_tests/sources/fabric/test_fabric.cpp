/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/mocks/mock_driver_info.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/fabric/fabric.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using FabricVertexFixture = Test<MultiDeviceFixture>;

TEST_F(FabricVertexFixture, WhenDevicesAreCreatedThenVerifyFabricVerticesAreCreated) {

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricVertexGetExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(count, numRootDevices);
    std::vector<ze_fabric_vertex_handle_t> vertexHandles;
    vertexHandles.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricVertexGetExp(driverHandle->toHandle(), &count, vertexHandles.data()));
    for (const auto &handle : vertexHandles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(FabricVertexFixture, givenDevicesAreCreatedWhenzeFabricVertexGetSubVerticesExpIsCalledThenValidSubVerticesAreReturned) {

    uint32_t count = 0;
    L0::zeFabricVertexGetExp(driverHandle->toHandle(), &count, nullptr);
    EXPECT_EQ(count, numRootDevices);
    std::vector<ze_fabric_vertex_handle_t> vertexHandles;
    vertexHandles.resize(count);
    L0::zeFabricVertexGetExp(driverHandle->toHandle(), &count, vertexHandles.data());
    for (const auto &handle : vertexHandles) {
        uint32_t subVertexCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricVertexGetSubVerticesExp(handle, &subVertexCount, nullptr));
        EXPECT_EQ(subVertexCount, numSubDevices);
        std::vector<ze_fabric_vertex_handle_t> subVertexHandles;
        subVertexHandles.resize(subVertexCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricVertexGetSubVerticesExp(handle, &subVertexCount, subVertexHandles.data()));
        for (const auto &handle : subVertexHandles) {
            EXPECT_NE(handle, nullptr);
        }
    }
}

TEST_F(FabricVertexFixture, givenFabricVerticesAreCreatedWhenzeFabricVertexGetPropertiesExpIsCalledThenValidPropertiesAreReturned) {

    // Setup pci address for all devices
    for (auto l0Device : driverHandle->devices) {
        auto deviceImp = static_cast<L0::DeviceImp *>(l0Device);
        for (auto l0SubDevice : deviceImp->subDevices) {
            auto subDeviceImp = static_cast<L0::DeviceImp *>(l0SubDevice);
            NEO::DriverInfoMock *driverInfo = new DriverInfoMock();
            driverInfo->setPciBusInfo({0, 1, 2, 3});
            subDeviceImp->driverInfo.reset(driverInfo);
            subDeviceImp->fabricVertex.reset(FabricVertex::createFromDevice(l0SubDevice));
        }
        NEO::DriverInfoMock *driverInfo = new DriverInfoMock();
        driverInfo->setPciBusInfo({0, 1, 2, 3});
        deviceImp->driverInfo.reset(driverInfo);
        deviceImp->fabricVertex.reset(FabricVertex::createFromDevice(l0Device));
    }

    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> vertexHandles;
    vertexHandles.resize(numRootDevices);
    std::vector<ze_fabric_vertex_handle_t> subVertexHandles;
    subVertexHandles.resize(numSubDevices);
    L0::zeFabricVertexGetExp(driverHandle->toHandle(), &count, nullptr);
    L0::zeFabricVertexGetExp(driverHandle->toHandle(), &count, vertexHandles.data());

    // Verify vertex properties are same as device properties
    auto verifyProperties = [](const ze_fabric_vertex_exp_properties_t &vertexProperties,
                               const ze_device_properties_t &deviceProperties,
                               const ze_pci_ext_properties_t &pciProperties) {
        EXPECT_TRUE(0 == std::memcmp(vertexProperties.uuid.id, deviceProperties.uuid.id, sizeof(vertexProperties.uuid.id)));
        if (deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) {
            EXPECT_EQ(vertexProperties.type, ZE_FABRIC_VERTEX_EXP_TYPE_SUBEVICE);
        } else {
            EXPECT_EQ(vertexProperties.type, ZE_FABRIC_VERTEX_EXP_TYPE_DEVICE);
        }
        EXPECT_FALSE(vertexProperties.remote);
        EXPECT_EQ(vertexProperties.address.domain, pciProperties.address.domain);
        EXPECT_EQ(vertexProperties.address.bus, pciProperties.address.bus);
        EXPECT_EQ(vertexProperties.address.device, pciProperties.address.device);
        EXPECT_EQ(vertexProperties.address.function, pciProperties.address.function);
    };

    for (const auto &handle : vertexHandles) {
        uint32_t subVertexCount = 0;
        ze_fabric_vertex_exp_properties_t vertexProperties = {};
        ze_device_properties_t deviceProperties = {};
        ze_pci_ext_properties_t pciProperties = {};
        ze_device_handle_t hDevice = {};

        vertexProperties.stype = ZE_STRUCTURE_TYPE_FABRIC_VERTEX_EXP_PROPERTIES;
        deviceProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        pciProperties.stype = ZE_STRUCTURE_TYPE_PCI_EXT_PROPERTIES;

        L0::zeFabricVertexGetSubVerticesExp(handle, &subVertexCount, nullptr);
        L0::zeFabricVertexGetSubVerticesExp(handle, &subVertexCount, subVertexHandles.data());

        // Verify properties of sub vertices
        for (const auto &subVertexHandle : subVertexHandles) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricVertexGetPropertiesExp(subVertexHandle, &vertexProperties));
            L0::zeFabricVertexGetDeviceExp(subVertexHandle, &hDevice);
            EXPECT_NE(hDevice, nullptr);
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetProperties(hDevice, &deviceProperties));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeDevicePciGetPropertiesExt(hDevice, &pciProperties));
            verifyProperties(vertexProperties, deviceProperties, pciProperties);
        }

        // Verify properties of Vertices
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricVertexGetPropertiesExp(handle, &vertexProperties));
        L0::zeFabricVertexGetDeviceExp(handle, &hDevice);
        EXPECT_NE(hDevice, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetProperties(hDevice, &deviceProperties));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeDevicePciGetPropertiesExt(hDevice, &pciProperties));
        verifyProperties(vertexProperties, deviceProperties, pciProperties);
    }
}

} // namespace ult
} // namespace L0