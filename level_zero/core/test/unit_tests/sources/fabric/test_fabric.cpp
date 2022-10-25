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
#include "level_zero/core/source/fabric/fabric_device_interface.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
class FabricDeviceMdfi;
namespace ult {

class MockFabricDeviceInterface {

    ze_result_t enumerate() { return ZE_RESULT_SUCCESS; }
    bool getEdgeProperty(FabricVertex *neighborVertex, ze_fabric_edge_exp_properties_t &edgeProperty) {

        if (mockEdgeProperties.size() > mockEdgePropertiesCounter) {
            edgeProperty = mockEdgeProperties[mockEdgePropertiesCounter];
        }
        return getEdgePropertyResult;
    }

    std::vector<ze_fabric_edge_exp_properties_t> mockEdgeProperties = {};
    uint32_t mockEdgePropertiesCounter = 0;
    bool getEdgePropertyResult = true;
};

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

    // Delete existing fabric vertices
    for (auto fabricVertex : driverHandle->fabricVertices) {
        delete fabricVertex;
    }
    driverHandle->fabricVertices.clear();

    // Setup pci address for all devices
    for (auto l0Device : driverHandle->devices) {
        auto deviceImp = static_cast<L0::DeviceImp *>(l0Device);

        for (auto l0SubDevice : deviceImp->subDevices) {

            auto subDeviceImp = static_cast<L0::DeviceImp *>(l0SubDevice);
            NEO::DriverInfoMock *driverInfo = new DriverInfoMock();
            driverInfo->setPciBusInfo({0, 1, 2, 3});
            subDeviceImp->driverInfo.reset(driverInfo);
        }

        NEO::DriverInfoMock *driverInfo = new DriverInfoMock();
        driverInfo->setPciBusInfo({0, 1, 2, 3});
        deviceImp->driverInfo.reset(driverInfo);
        driverHandle->fabricVertices.push_back(FabricVertex::createFromDevice(l0Device));
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

TEST(FabricEngineInstanceTest, GivenEngineInstancedDeviceWhenFabricVerticesAreCreatedThenSkipCreationForEngineInstanced) {

    auto hwInfo = *NEO::defaultHwInfo.get();
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
    std::vector<std::unique_ptr<NEO::Device>> devices(1);
    devices[0].reset(static_cast<NEO::Device *>(NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(&hwInfo, executionEnvironment, 0u)));

    auto mockDevice = static_cast<NEO::MockDevice *>(devices[0].get());
    NEO::SubDevice *subDevice = static_cast<NEO::SubDevice *>(mockDevice->createEngineInstancedSubDevice(0, defaultHwInfo->capabilityTable.defaultEngineType));
    mockDevice->subdevices.push_back(subDevice);

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    ze_result_t res = driverHandle->initialize(std::move(devices));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    res = driverHandle->fabricVertexGetExp(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    phVertices.resize(count);
    res = driverHandle->fabricVertexGetExp(&count, phVertices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(driverHandle->fabricVertices.size(), 1u);
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricVertices[0]->getSubVertices(&count, nullptr));
    EXPECT_EQ(count, 0u);
}

TEST_F(FabricVertexFixture, GivenDevicesAreCreatedWhenZeDeviceGetFabricVertexExpIsCalledThenExpectValidVertices) {

    for (auto l0Device : driverHandle->devices) {
        ze_fabric_vertex_handle_t hVertex = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeDeviceGetFabricVertexExp(l0Device->toHandle(), &hVertex));
        EXPECT_NE(hVertex, nullptr);

        auto deviceImp = static_cast<L0::DeviceImp *>(l0Device);
        for (auto l0SubDevice : deviceImp->subDevices) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeDeviceGetFabricVertexExp(l0SubDevice->toHandle(), &hVertex));
            EXPECT_NE(hVertex, nullptr);
        }
    }
}

TEST_F(FabricVertexFixture, GivenDevicesAreCreatedWhenFabricVertexIsNotSetToDeviceThenZeDeviceGetFabricVertexExpReturnsError) {
    auto l0Device = driverHandle->devices[0];

    ze_fabric_vertex_handle_t hVertex = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeDeviceGetFabricVertexExp(l0Device->toHandle(), &hVertex));
    EXPECT_NE(hVertex, nullptr);

    auto deviceImp = static_cast<L0::DeviceImp *>(l0Device);
    deviceImp->setFabricVertex(nullptr);

    hVertex = nullptr;
    EXPECT_EQ(ZE_RESULT_EXP_ERROR_DEVICE_IS_NOT_VERTEX, L0::zeDeviceGetFabricVertexExp(l0Device->toHandle(), &hVertex));
    EXPECT_EQ(hVertex, nullptr);
}

using FabricEdgeFixture = Test<MultiDeviceFixture>;

TEST_F(FabricEdgeFixture, GivenFabricVerticesAreCreatedWhenZeFabricEdgeGetExpIsCalledThenReturnSuccess) {

    // initialize
    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    ze_result_t res = driverHandle->fabricVertexGetExp(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    phVertices.resize(count);
    res = driverHandle->fabricVertexGetExp(&count, phVertices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    // Delete existing fabric edges
    for (auto edge : driverHandle->fabricEdges) {
        delete edge;
    }
    driverHandle->fabricEdges.clear();

    ze_fabric_edge_exp_properties_t dummyProperties = {};
    driverHandle->fabricEdges.push_back(FabricEdge::create(driverHandle->fabricVertices[0], driverHandle->fabricVertices[1], dummyProperties));
    driverHandle->fabricEdges.push_back(FabricEdge::create(driverHandle->fabricVertices[0], driverHandle->fabricVertices[1], dummyProperties));
    driverHandle->fabricEdges.push_back(FabricEdge::create(driverHandle->fabricVertices[0], driverHandle->fabricVertices[1], dummyProperties));

    std::vector<ze_fabric_edge_handle_t> edgeHandles(10);
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(driverHandle->fabricVertices[0]->toHandle(),
                                                        driverHandle->fabricVertices[1]->toHandle(),
                                                        &count,
                                                        edgeHandles.data()));
    EXPECT_EQ(count, 3u);
    count = 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(driverHandle->fabricVertices[1]->toHandle(),
                                                        driverHandle->fabricVertices[0]->toHandle(),
                                                        &count,
                                                        edgeHandles.data()));
}

TEST_F(FabricEdgeFixture, GivenFabricEdgesAreCreatedWhenZeFabricEdgeGetVerticesExpIsCalledThenReturnCorrectVertices) {
    // initialize
    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    ze_result_t res = driverHandle->fabricVertexGetExp(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    phVertices.resize(count);
    res = driverHandle->fabricVertexGetExp(&count, phVertices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    // Delete existing fabric edges
    for (auto edge : driverHandle->fabricEdges) {
        delete edge;
    }
    driverHandle->fabricEdges.clear();

    ze_fabric_edge_exp_properties_t dummyProperties = {};
    driverHandle->fabricEdges.push_back(FabricEdge::create(driverHandle->fabricVertices[0], driverHandle->fabricVertices[1], dummyProperties));

    std::vector<ze_fabric_edge_handle_t> edgeHandles(10);
    count = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(driverHandle->fabricVertices[1]->toHandle(),
                                                        driverHandle->fabricVertices[0]->toHandle(),
                                                        &count,
                                                        edgeHandles.data()));
    ze_fabric_vertex_handle_t hVertexA = nullptr;
    ze_fabric_vertex_handle_t hVertexB = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetVerticesExp(edgeHandles[0], &hVertexA, &hVertexB));
    EXPECT_EQ(hVertexA, driverHandle->fabricVertices[0]);
    EXPECT_EQ(hVertexB, driverHandle->fabricVertices[1]);
}

TEST_F(FabricEdgeFixture, GivenFabricEdgesAreCreatedWhenZeFabricEdgeGetPropertiesExpIsCalledThenReturnCorrectProperties) {
    // initialize
    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    ze_result_t res = driverHandle->fabricVertexGetExp(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    phVertices.resize(count);
    res = driverHandle->fabricVertexGetExp(&count, phVertices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    // Delete existing fabric edges
    for (auto edge : driverHandle->fabricEdges) {
        delete edge;
    }
    driverHandle->fabricEdges.clear();

    ze_fabric_edge_exp_properties_t properties = {};
    properties.bandwidth = 10;
    properties.latency = 20;
    driverHandle->fabricEdges.push_back(FabricEdge::create(driverHandle->fabricVertices[0], driverHandle->fabricVertices[1], properties));

    std::vector<ze_fabric_edge_handle_t> edgeHandles(10);
    count = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(driverHandle->fabricVertices[1]->toHandle(),
                                                        driverHandle->fabricVertices[0]->toHandle(),
                                                        &count,
                                                        edgeHandles.data()));
    ze_fabric_edge_exp_properties_t getProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edgeHandles[0], &getProperties));
    EXPECT_EQ(getProperties.bandwidth, 10u);
    EXPECT_EQ(getProperties.latency, 20u);
}

TEST_F(FabricEdgeFixture, GivenMdfiLinksAreAvailableWhenEdgesAreCreatedThenVerifyThatBiDirectionalEdgesAreNotCreated) {
    // initialize
    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    ze_result_t res = driverHandle->fabricVertexGetExp(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    phVertices.resize(count);
    res = driverHandle->fabricVertexGetExp(&count, phVertices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &fabricSubVertex1 = driverHandle->fabricVertices[0]->subVertices[1];
    auto fabricDeviceMdfi = static_cast<FabricDeviceMdfi *>(fabricSubVertex1->pFabricDeviceInterfaces[FabricDeviceInterface::Type::Mdfi].get());

    ze_fabric_edge_exp_properties_t unusedProperty = {};
    EXPECT_FALSE(fabricDeviceMdfi->getEdgeProperty(driverHandle->fabricVertices[0]->subVertices[0], unusedProperty));
}

} // namespace ult
} // namespace L0
