/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_info.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/extensions/public/ze_exp_ext.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/fabric/fabric.h"
#include "level_zero/core/source/fabric/fabric_device_interface.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
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

using FabricVertexFixture = Test<MultiDeviceWithFabricFixture>;

TEST_F(FabricVertexFixture, whenDevicesAreCreatedThenVerifyFabricVerticesAreCreated) {

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
        auto device = l0Device;

        for (auto l0SubDevice : device->subDevices) {

            auto subDevice = l0SubDevice;
            NEO::DriverInfoMock *driverInfo = new DriverInfoMock();
            driverInfo->setPciBusInfo({0, 1, 2, 3});
            subDevice->driverInfo.reset(driverInfo);
        }

        NEO::DriverInfoMock *driverInfo = new DriverInfoMock();
        driverInfo->setPciBusInfo({0, 1, 2, 3});
        device->driverInfo.reset(driverInfo);
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
            EXPECT_EQ(vertexProperties.type, ZE_FABRIC_VERTEX_EXP_TYPE_SUBDEVICE);
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

struct FabricEngineInstanceFixture : public Test<MultiDeviceWithFabricFixture> {
    void SetUp() override {
        numRootDevices = 1u;
        numSubDevices = 0u;
        MultiDeviceWithFabricFixture::setUp();
    }
};

TEST_F(FabricEngineInstanceFixture, givenSubDeviceWhenFabricVerticesAreCreatedThenSkipCreationForSubDevice) {

    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    ze_result_t res = driverHandle->fabricVertexGetExp(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    phVertices.resize(count);
    res = driverHandle->fabricVertexGetExp(&count, phVertices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(driverHandle->fabricVertices.size(), 1u);
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricVertices[0]->getSubVertices(&count, nullptr));
    EXPECT_EQ(count, 0u);
}

TEST_F(FabricVertexFixture, givenDevicesAreCreatedWhenZeDeviceGetFabricVertexExpIsCalledThenExpectValidVertices) {

    for (auto l0Device : driverHandle->devices) {
        ze_fabric_vertex_handle_t hVertex = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeDeviceGetFabricVertexExp(l0Device->toHandle(), &hVertex));
        EXPECT_NE(hVertex, nullptr);

        auto device = l0Device;
        for (auto l0SubDevice : device->subDevices) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeDeviceGetFabricVertexExp(l0SubDevice->toHandle(), &hVertex));
            EXPECT_NE(hVertex, nullptr);
        }
    }
}

TEST_F(FabricVertexFixture, givenDevicesAreCreatedWhenFabricVertexIsNotSetToDeviceThenZeDeviceGetFabricVertexExpReturnsError) {
    auto l0Device = driverHandle->devices[0];

    ze_fabric_vertex_handle_t hVertex = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeDeviceGetFabricVertexExp(l0Device->toHandle(), &hVertex));
    EXPECT_NE(hVertex, nullptr);

    auto device = l0Device;
    device->setFabricVertex(nullptr);

    hVertex = nullptr;
    EXPECT_EQ(ZE_RESULT_EXP_ERROR_DEVICE_IS_NOT_VERTEX, L0::zeDeviceGetFabricVertexExp(l0Device->toHandle(), &hVertex));
    EXPECT_EQ(hVertex, nullptr);
}

TEST(FabricVertexTestFixture, givenCompositeHierarchyWhenFabricVerticesGetExpIsCalledCorrectVerticesAreReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0,1.1,2");
    MultiDeviceFixtureCompositeHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->fabricVertexGetExp(&count, nullptr), ZE_RESULT_SUCCESS);
    uint32_t expectedVertexes = 3u;
    EXPECT_EQ(count, expectedVertexes);

    phVertices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->fabricVertexGetExp(&count, phVertices.data()), ZE_RESULT_SUCCESS);

    // Device 0 associated with value 0 in mask
    ze_device_handle_t hDevice{};
    EXPECT_EQ(L0::zeFabricVertexGetDeviceExp(phVertices[0], &hDevice), ZE_RESULT_SUCCESS);
    Device *device = static_cast<Device *>(hDevice);
    EXPECT_FALSE(device->isSubdevice);

    uint32_t countSubDevices = 0;
    EXPECT_EQ(L0::zeFabricVertexGetSubVerticesExp(phVertices[0], &countSubDevices, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(countSubDevices, multiDeviceFixture.numSubDevices);

    std::vector<ze_fabric_vertex_handle_t> phSubvertices(countSubDevices);
    EXPECT_EQ(L0::zeFabricVertexGetSubVerticesExp(phVertices[0], &countSubDevices, phSubvertices.data()), ZE_RESULT_SUCCESS);
    for (auto subVertex : phSubvertices) {
        ze_device_handle_t hSubDevice{};
        EXPECT_EQ(L0::zeFabricVertexGetDeviceExp(subVertex, &hSubDevice), ZE_RESULT_SUCCESS);
        Device *subDevice = static_cast<Device *>(hSubDevice);
        EXPECT_TRUE(subDevice->isSubdevice);
    }

    // Device 1 associated with value 1.1 in mask
    EXPECT_EQ(L0::zeFabricVertexGetDeviceExp(phVertices[1], &hDevice), ZE_RESULT_SUCCESS);
    device = static_cast<Device *>(hDevice);
    EXPECT_FALSE(device->isSubdevice);

    phSubvertices.clear();
    countSubDevices = 0;
    EXPECT_EQ(L0::zeFabricVertexGetSubVerticesExp(phVertices[1], &countSubDevices, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(countSubDevices, 0u);

    // Device 2 associated with value 2 in mask
    EXPECT_EQ(L0::zeFabricVertexGetDeviceExp(phVertices[1], &hDevice), ZE_RESULT_SUCCESS);
    device = static_cast<Device *>(hDevice);
    EXPECT_FALSE(device->isSubdevice);

    phSubvertices.clear();
    countSubDevices = 0;
    EXPECT_EQ(L0::zeFabricVertexGetSubVerticesExp(phVertices[2], &countSubDevices, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(countSubDevices, multiDeviceFixture.numSubDevices);

    multiDeviceFixture.tearDown();
}

TEST(FabricVertexTestFixture, givenFlatHierarchyWhenFabricVerticesGetExpIsCalledCorrectVerticesAreReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0,1.1,2");
    MultiDeviceFixtureFlatHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->fabricVertexGetExp(&count, nullptr), ZE_RESULT_SUCCESS);
    // only 2 vertexes for mask "0,1.1,2":
    // 0 and 2
    // 1.1 is ignored in FlatHierarchy
    uint32_t expectedVertexes = 2u;
    EXPECT_EQ(count, expectedVertexes);

    // Requesting for a reduced count
    phVertices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->fabricVertexGetExp(&count, phVertices.data()), ZE_RESULT_SUCCESS);

    ze_device_handle_t hDevice{};
    // Device 0 associated with value 0 in mask
    EXPECT_EQ(L0::zeFabricVertexGetDeviceExp(phVertices[0], &hDevice), ZE_RESULT_SUCCESS);
    Device *device = static_cast<Device *>(hDevice);
    EXPECT_FALSE(device->isSubdevice);

    // Device 1 associated with value 2 in mask
    EXPECT_EQ(L0::zeFabricVertexGetDeviceExp(phVertices[1], &hDevice), ZE_RESULT_SUCCESS);
    device = static_cast<Device *>(hDevice);
    EXPECT_FALSE(device->isSubdevice);

    multiDeviceFixture.tearDown();
}

TEST(FabricVertexTestFixture, givenCombinedHierarchyWhenFabricVerticesGetExpIsCalledCorrectVerticesAreReturned2) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0,1.1,2");
    MultiDeviceFixtureCombinedHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->fabricVertexGetExp(&count, nullptr), ZE_RESULT_SUCCESS);
    uint32_t expectedVertexes = 2u;
    EXPECT_EQ(count, expectedVertexes);

    phVertices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->fabricVertexGetExp(&count, phVertices.data()), ZE_RESULT_SUCCESS);

    multiDeviceFixture.tearDown();
}

using FabricEdgeFixture = Test<MultiDeviceWithFabricFixture>;

TEST_F(FabricEdgeFixture, givenFabricVerticesAreCreatedWhenZeFabricEdgeGetExpIsCalledThenReturnSuccess) {

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

TEST_F(FabricEdgeFixture, givenFabricVerticesAreCreatedForIndirectEdgesWhenZeFabricEdgeGetExpIsCalledThenReturnSuccess) {
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

    for (auto edge : driverHandle->fabricIndirectEdges) {
        delete edge;
    }
    driverHandle->fabricIndirectEdges.clear();

    ze_fabric_edge_exp_properties_t dummyProperties = {};
    driverHandle->fabricEdges.push_back(FabricEdge::create(driverHandle->fabricVertices[0], driverHandle->fabricVertices[1], dummyProperties));
    driverHandle->fabricEdges.push_back(FabricEdge::create(driverHandle->fabricVertices[1], driverHandle->fabricVertices[0], dummyProperties));
    driverHandle->fabricIndirectEdges.push_back(FabricEdge::create(driverHandle->fabricVertices[0], driverHandle->fabricVertices[1], dummyProperties));
    driverHandle->fabricIndirectEdges.push_back(FabricEdge::create(driverHandle->fabricVertices[1], driverHandle->fabricVertices[0], dummyProperties));
    driverHandle->fabricIndirectEdges.push_back(FabricEdge::create(driverHandle->fabricVertices[0], driverHandle->fabricVertices[1], dummyProperties));
    driverHandle->fabricIndirectEdges.push_back(FabricEdge::create(driverHandle->fabricVertices[1], driverHandle->fabricVertices[0], dummyProperties));

    std::vector<ze_fabric_edge_handle_t> edgeHandles(10);
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(driverHandle->fabricVertices[0]->toHandle(),
                                                        driverHandle->fabricVertices[1]->toHandle(),
                                                        &count,
                                                        edgeHandles.data()));
    EXPECT_EQ(count, 4u);
    count = 3;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(driverHandle->fabricVertices[1]->toHandle(),
                                                        driverHandle->fabricVertices[0]->toHandle(),
                                                        &count,
                                                        edgeHandles.data()));

    for (auto edge : driverHandle->fabricEdges) {
        delete edge;
    }
    driverHandle->fabricEdges.clear();

    for (auto edge : driverHandle->fabricIndirectEdges) {
        delete edge;
    }
    driverHandle->fabricIndirectEdges.clear();
}

TEST_F(FabricEdgeFixture, givenFabricEdgesAreCreatedWhenZeFabricEdgeGetVerticesExpIsCalledThenReturnCorrectVertices) {
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

TEST_F(FabricEdgeFixture, givenFabricEdgesAreCreatedWhenZeFabricEdgeGetPropertiesExpIsCalledThenReturnCorrectProperties) {
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

TEST_F(FabricEdgeFixture, givenMdfiLinksAreAvailableWhenEdgesAreCreatedThenVerifyThatBiDirectionalEdgesAreNotCreated) {
    // initialize
    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    ze_result_t res = driverHandle->fabricVertexGetExp(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    phVertices.resize(count);
    res = driverHandle->fabricVertexGetExp(&count, phVertices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &fabricSubVertex1 = driverHandle->fabricVertices[0]->subVertices[1];
    auto fabricDeviceMdfi = static_cast<FabricDeviceMdfi *>(fabricSubVertex1->pFabricDeviceInterfaces[FabricDeviceInterface::Type::mdfi].get());

    ze_fabric_edge_exp_properties_t unusedProperty = {};
    EXPECT_FALSE(fabricDeviceMdfi->getEdgeProperty(driverHandle->fabricVertices[0]->subVertices[0], unusedProperty));
}

TEST(FabricVertexTestFixture, givenFlatHierarchyWhenGetSubVerticesIsCalledThenZeroSubVerticesReturned) {
    DebugManagerStateRestore restorer;
    MultiDeviceFixtureFlatHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->fabricVertexGetExp(&count, nullptr), ZE_RESULT_SUCCESS);
    phVertices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->fabricVertexGetExp(&count, phVertices.data()), ZE_RESULT_SUCCESS);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t subVertexCount = 0;
        EXPECT_EQ(L0::zeFabricVertexGetSubVerticesExp(phVertices[i], &subVertexCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(subVertexCount, 0u);
    }

    multiDeviceFixture.tearDown();
}

TEST(FabricVertexTestFixture, givenExposeSingleDeviceModeWhenGetSubVerticesIsCalledThenZeroSubVerticesReturned) {
    DebugManagerStateRestore restorer;
    MultiDeviceFixtureCompositeHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    for (auto l0Device : multiDeviceFixture.driverHandle->devices) {
        auto neoDevice = l0Device->getNEODevice();
        neoDevice->getRootDeviceEnvironmentRef().setExposeSingleDeviceMode(true);
    }

    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->fabricVertexGetExp(&count, nullptr), ZE_RESULT_SUCCESS);
    phVertices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->fabricVertexGetExp(&count, phVertices.data()), ZE_RESULT_SUCCESS);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t subVertexCount = 0;
        EXPECT_EQ(L0::zeFabricVertexGetSubVerticesExp(phVertices[i], &subVertexCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(subVertexCount, 0u);
    }

    multiDeviceFixture.tearDown();
}

TEST_F(FabricVertexFixture, givenVertexInfoWhenCreateFromVertexInfoCalledThenUuidIsCopiedCorrectly) {
    NEO::FabricVertexInfo vertexInfo{};
    vertexInfo.fabricId = 7u;
    vertexInfo.type = NEO::FabricVertexType::device;
    for (uint8_t i = 0; i < sizeof(vertexInfo.uuid); i++) {
        vertexInfo.uuid[i] = static_cast<uint8_t>(i + 1);
    }
    vertexInfo.pciBdf = 0u;

    auto *device = driverHandle->devices[0];
    auto fabricVertex = FabricVertex::createFromVertexInfo(device, vertexInfo);
    ASSERT_NE(nullptr, fabricVertex);

    ze_fabric_vertex_exp_properties_t props{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, fabricVertex->getProperties(&props));
    EXPECT_EQ(0, memcmp(props.uuid.id, vertexInfo.uuid, sizeof(vertexInfo.uuid)));

    delete fabricVertex;
}

TEST_F(FabricVertexFixture, givenVertexInfoWhenCreateFromVertexInfoCalledThenTypeAndFlagsAreSetCorrectly) {
    NEO::FabricVertexInfo vertexInfo{};
    vertexInfo.fabricId = 3u;
    vertexInfo.type = NEO::FabricVertexType::subdevice;
    vertexInfo.pciBdf = 0u;

    auto *device = driverHandle->devices[0];
    auto fabricVertex = FabricVertex::createFromVertexInfo(device, vertexInfo);
    ASSERT_NE(nullptr, fabricVertex);

    ze_fabric_vertex_exp_properties_t props{};
    fabricVertex->getProperties(&props);

    EXPECT_EQ(ZE_STRUCTURE_TYPE_FABRIC_VERTEX_EXP_PROPERTIES, props.stype);
    EXPECT_EQ(ZE_FABRIC_VERTEX_EXP_TYPE_SUBDEVICE, props.type);
    EXPECT_FALSE(props.remote);

    delete fabricVertex;
}

TEST_F(FabricVertexFixture, givenVertexInfoWhenCreateFromVertexInfoCalledThenFabricIdAndDeviceAreSet) {
    NEO::FabricVertexInfo vertexInfo{};
    vertexInfo.fabricId = 55u;
    vertexInfo.type = NEO::FabricVertexType::device;
    vertexInfo.pciBdf = 0u;

    auto *device = driverHandle->devices[0];
    auto fabricVertex = FabricVertex::createFromVertexInfo(device, vertexInfo);
    ASSERT_NE(nullptr, fabricVertex);

    EXPECT_EQ(55u, fabricVertex->fabricId);
    EXPECT_EQ(device, fabricVertex->device);

    delete fabricVertex;
}

TEST_F(FabricVertexFixture, givenVertexInfoWhenCreateFromVertexInfoCalledThenPciBdfIsDecodedCorrectly) {
    NEO::FabricVertexInfo vertexInfo{};
    vertexInfo.fabricId = 1u;
    vertexInfo.type = NEO::FabricVertexType::device;
    constexpr uint32_t expectedDomain = 0x0001u;
    constexpr uint32_t expectedBus = 0x02u;
    constexpr uint32_t expectedDevice = 0x1fu;
    constexpr uint32_t expectedFunction = 0x7u;
    vertexInfo.pciBdf = (expectedDomain << 16) | (expectedBus << 8) | (expectedDevice << 3) | expectedFunction;

    auto *l0Device = driverHandle->devices[0];
    auto fabricVertex = FabricVertex::createFromVertexInfo(l0Device, vertexInfo);
    ASSERT_NE(nullptr, fabricVertex);

    ze_fabric_vertex_exp_properties_t props{};
    fabricVertex->getProperties(&props);

    EXPECT_EQ(0x0001u, props.address.domain);
    EXPECT_EQ(0x02u, props.address.bus);
    EXPECT_EQ(expectedDevice, props.address.device);
    EXPECT_EQ(0x7u, props.address.function);

    delete fabricVertex;
}

} // namespace ult
} // namespace L0
