/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/fabric/fabric.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_fabric.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

class MockIoctlHelperIafTest : public NEO::IoctlHelperPrelim20 {
  public:
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
    bool getFabricLatency(uint32_t fabricId, uint32_t &latency, uint32_t &bandwidth) override {
        latency = 1;
        bandwidth = 10;
        return mockFabricLatencyReturn;
    }

    bool mockFabricLatencyReturn = true;
};

using FabricMultiHopEdgeFixture = Test<MultiDeviceFixture>;

TEST_F(FabricMultiHopEdgeFixture, GivenMultipleDevicesAndSubDevicesThenMultiHopEdgesAreCreated) {
    for (auto &device : driverHandle->devices) {
        auto executionEnvironment = device->getNEODevice()->getExecutionEnvironment();
        auto &rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()];
        auto osInterface = new OSInterface();
        auto drmMock = new DrmMockResources(*rootDeviceEnvironment);
        drmMock->ioctlHelper.reset(new MockIoctlHelperIafTest(*drmMock));
        rootDeviceEnvironment->osInterface.reset(osInterface);
        executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
    }

    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricVertexGetExp(&count, nullptr));
    phVertices.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricVertexGetExp(&count, phVertices.data()));

    // Physical connections between the four sub-devices of two root devices
    //
    //         +---------------------------------+
    //         |                                 |
    //      +--v--+  +-----+         +-----+  +--v--+
    //      |D0.S0<==>D0.S1|         |D1.S0<==>D1.S1|
    //      +-----+  +--^--+         +--^--+  +-----+
    //                  |               |                <==>: MDFI
    //                  +---------------+                <-->: XeLink
    //
    // IAF port connection configuration
    //    Device | SubDevice | Port -- Connected to -- Device | SubDevice | Port | Bandwidth
    //      0          0        1                        1          1        2        15
    //      0          1        3                        1          0        4        17
    //
    // Guids:
    //  Device 0, subdevice 0 = 0xA
    //  Device 0, subdevice 1 = 0xAB
    //  Device 1, subdevice 0 = 0xABC
    //  Device 1, subdevice 1 = 0xABCD

    std::vector<FabricPortConnection> connection00To11;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(0, 0, 1);
        connection.neighbourPortNumber = 2;
        connection.neighbourGuid = 0xABCD;
        connection.bandwidthInBytesPerNanoSecond = 15;
        connection.isDuplex = true;
        connection00To11.push_back(connection);
    }

    std::vector<FabricPortConnection> connection01To10;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(0, 1, 3);
        connection.neighbourPortNumber = 4;
        connection.neighbourGuid = 0xABC;
        connection.bandwidthInBytesPerNanoSecond = 17;
        connection.isDuplex = true;
        connection01To10.push_back(connection);
    }

    std::vector<FabricPortConnection> connection11To00;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(1, 1, 2);
        connection.neighbourPortNumber = 1;
        connection.neighbourGuid = 0xA;
        connection.bandwidthInBytesPerNanoSecond = 15;
        connection.isDuplex = true;
        connection11To00.push_back(connection);
    }

    std::vector<FabricPortConnection> connection10To01;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(1, 0, 4);
        connection.neighbourPortNumber = 3;
        connection.neighbourGuid = 0xAB;
        connection.bandwidthInBytesPerNanoSecond = 17;
        connection.isDuplex = true;
        connection10To01.push_back(connection);
    }

    auto &fabricVertex0 = driverHandle->fabricVertices[0];
    {
        auto fabricDeviceIaf = static_cast<FabricDeviceIaf *>(fabricVertex0->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());

        auto &fabricSubDeviceIaf0 = fabricDeviceIaf->subDeviceIafs[0];
        fabricSubDeviceIaf0->connections.clear();
        fabricSubDeviceIaf0->connections = connection00To11;
        fabricSubDeviceIaf0->guid = 0xA;

        auto &fabricSubDeviceIaf1 = fabricDeviceIaf->subDeviceIafs[1];
        fabricSubDeviceIaf1->connections.clear();
        fabricSubDeviceIaf1->connections = connection01To10;
        fabricSubDeviceIaf1->guid = 0xAB;

        auto &fabricVertex00 = fabricVertex0->subVertices[0];
        auto fabricSubDeviceIaf00 = static_cast<FabricSubDeviceIaf *>(fabricVertex00->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());
        fabricSubDeviceIaf00->connections.clear();
        fabricSubDeviceIaf00->connections = connection00To11;
        fabricSubDeviceIaf00->guid = 0xA;

        auto &fabricVertex01 = fabricVertex0->subVertices[1];
        auto fabricSubDeviceIaf01 = static_cast<FabricSubDeviceIaf *>(fabricVertex01->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());
        fabricSubDeviceIaf01->connections.clear();
        fabricSubDeviceIaf01->connections = connection01To10;
        fabricSubDeviceIaf01->guid = 0xAB;
    }

    auto &fabricVertex1 = driverHandle->fabricVertices[1];
    {
        auto fabricDeviceIaf = static_cast<FabricDeviceIaf *>(fabricVertex1->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());

        auto &fabricSubDeviceIaf0 = fabricDeviceIaf->subDeviceIafs[0];
        fabricSubDeviceIaf0->connections.clear();
        fabricSubDeviceIaf0->connections = connection10To01;
        fabricSubDeviceIaf0->guid = 0xABC;

        auto &fabricSubDeviceIaf1 = fabricDeviceIaf->subDeviceIafs[1];
        fabricSubDeviceIaf1->connections.clear();
        fabricSubDeviceIaf1->connections = connection11To00;
        fabricSubDeviceIaf1->guid = 0xABCD;

        auto &fabricVertex10 = fabricVertex1->subVertices[0];
        auto fabricSubDeviceIaf10 = static_cast<FabricSubDeviceIaf *>(fabricVertex10->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());
        fabricSubDeviceIaf10->connections.clear();
        fabricSubDeviceIaf10->connections = connection10To01;
        fabricSubDeviceIaf10->guid = 0xABC;

        auto &fabricVertex11 = fabricVertex1->subVertices[1];
        auto fabricSubDeviceIaf11 = static_cast<FabricSubDeviceIaf *>(fabricVertex11->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());
        fabricSubDeviceIaf11->connections.clear();
        fabricSubDeviceIaf11->connections = connection11To00;
        fabricSubDeviceIaf11->guid = 0xABCD;
    }

    for (auto &edge : driverHandle->fabricEdges) {
        delete edge;
    }
    driverHandle->fabricEdges.clear();
    for (auto &edge : driverHandle->fabricIndirectEdges) {
        delete edge;
    }
    driverHandle->fabricIndirectEdges.clear();
    FabricEdge::createEdgesFromVertices(driverHandle->fabricVertices, driverHandle->fabricEdges, driverHandle->fabricIndirectEdges);

    constexpr size_t rootToRootDirect = 1;
    constexpr size_t subDeviceToRootDirect = 4;          // 2 roots, each to two subdevices
    constexpr size_t subDeviceToSubDeviceDirect = 4 + 2; // 4 MDFI + 2 pairs of subdevices
    constexpr size_t subDeviceToSubDeviceIndirect = 4;   // 2 pairs of subdevices, each has two directions (asymmetric)

    EXPECT_EQ(driverHandle->fabricEdges.size(), rootToRootDirect + subDeviceToRootDirect + subDeviceToSubDeviceDirect);
    EXPECT_EQ(driverHandle->fabricIndirectEdges.size(), subDeviceToSubDeviceIndirect);

    std::vector<ze_fabric_edge_handle_t> edges;
    ze_fabric_edge_exp_properties_t edgeProperties{};

    // Root to root
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    ze_fabric_vertex_handle_t vertexA = nullptr;
    ze_fabric_vertex_handle_t vertexB = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetVerticesExp(edges[0], &vertexA, &vertexB));
    EXPECT_EQ(vertexA, fabricVertex0);
    EXPECT_EQ(vertexB, fabricVertex1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 32u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    // Root to subdevice
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 17u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex0->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex0->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 15u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    // Subdevice to root
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex0->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex0->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 15u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    // Subdevice to subdevice (direct)
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[0], fabricVertex0->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[0], fabricVertex0->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 0u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_UNKNOWN);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex0->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex0->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 17u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    // Subdevice to subdevice (multi-hop)
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[0], fabricVertex1->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[0], fabricVertex1->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 17u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex0->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex0->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 15u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex1->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex1->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 15u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex0->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex0->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 17u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    for (auto &edge : driverHandle->fabricEdges) {
        delete edge;
    }
    driverHandle->fabricEdges.clear();
    for (auto &edge : driverHandle->fabricIndirectEdges) {
        delete edge;
    }
    driverHandle->fabricIndirectEdges.clear();
}

TEST_F(FabricMultiHopEdgeFixture, GivenSubDeviceChainConnectedThroughMDFIAndIAFThenMultiHopEdgesReportsTheMinimumBandwidth) {
    for (auto &device : driverHandle->devices) {
        auto executionEnvironment = device->getNEODevice()->getExecutionEnvironment();
        auto &rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()];
        auto osInterface = new OSInterface();
        auto drmMock = new DrmMockResources(*rootDeviceEnvironment);
        drmMock->ioctlHelper.reset(new MockIoctlHelperIafTest(*drmMock));
        rootDeviceEnvironment->osInterface.reset(osInterface);
        executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
    }

    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricVertexGetExp(&count, nullptr));
    phVertices.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricVertexGetExp(&count, phVertices.data()));

    // Physical connections between the six subdevices of the three root devices
    //
    // [D0.S1] <===> [D0.S0] <--XeLink(3)--> [D1.S0] <===> [D1.S1] <--XeLink(4)--> [D2.S0] <===> [D2.S1]
    //
    // Guids:
    //  Device 0, subdevice 0 = 0xA
    //  Device 1, subdevice 0 = 0xAB
    //  Device 1, subdevice 1 = 0xABC
    //  Device 2, subdevice 0 = 0xABCD

    std::vector<FabricPortConnection> connection00To10;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(0, 0, 0);
        connection.neighbourPortNumber = 1;
        connection.neighbourGuid = 0xAB;
        connection.bandwidthInBytesPerNanoSecond = 3;
        connection.isDuplex = true;
        connection00To10.push_back(connection);
    }

    std::vector<FabricPortConnection> connection10To00;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(1, 0, 1);
        connection.neighbourPortNumber = 0;
        connection.neighbourGuid = 0xA;
        connection.bandwidthInBytesPerNanoSecond = 3;
        connection.isDuplex = true;
        connection10To00.push_back(connection);
    }

    std::vector<FabricPortConnection> connection11To20;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(1, 1, 2);
        connection.neighbourPortNumber = 3;
        connection.neighbourGuid = 0xABCD;
        connection.bandwidthInBytesPerNanoSecond = 4;
        connection.isDuplex = true;
        connection11To20.push_back(connection);
    }

    std::vector<FabricPortConnection> connection20To11;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(2, 0, 3);
        connection.neighbourPortNumber = 2;
        connection.neighbourGuid = 0xABC;
        connection.bandwidthInBytesPerNanoSecond = 4;
        connection.isDuplex = true;
        connection20To11.push_back(connection);
    }

    auto &fabricVertex0 = driverHandle->fabricVertices[0];
    {
        auto fabricDeviceIaf = static_cast<FabricDeviceIaf *>(fabricVertex0->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());

        auto &fabricSubDeviceIaf0 = fabricDeviceIaf->subDeviceIafs[0];
        fabricSubDeviceIaf0->connections.clear();
        fabricSubDeviceIaf0->connections = connection00To10;
        fabricSubDeviceIaf0->guid = 0xA;

        auto &fabricVertex00 = fabricVertex0->subVertices[0];
        auto fabricSubDeviceIaf00 = static_cast<FabricSubDeviceIaf *>(fabricVertex00->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());
        fabricSubDeviceIaf00->connections.clear();
        fabricSubDeviceIaf00->connections = connection00To10;
        fabricSubDeviceIaf00->guid = 0xA;
    }

    auto &fabricVertex1 = driverHandle->fabricVertices[1];
    {
        auto fabricDeviceIaf = static_cast<FabricDeviceIaf *>(fabricVertex1->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());

        auto &fabricSubDeviceIaf0 = fabricDeviceIaf->subDeviceIafs[0];
        fabricSubDeviceIaf0->connections.clear();
        fabricSubDeviceIaf0->connections = connection10To00;
        fabricSubDeviceIaf0->guid = 0xAB;

        auto &fabricSubDeviceIaf1 = fabricDeviceIaf->subDeviceIafs[1];
        fabricSubDeviceIaf1->connections.clear();
        fabricSubDeviceIaf1->connections = connection11To20;
        fabricSubDeviceIaf1->guid = 0xABC;

        auto &fabricVertex10 = fabricVertex1->subVertices[0];
        auto fabricSubDeviceIaf10 = static_cast<FabricSubDeviceIaf *>(fabricVertex10->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());
        fabricSubDeviceIaf10->connections.clear();
        fabricSubDeviceIaf10->connections = connection10To00;
        fabricSubDeviceIaf10->guid = 0xAB;

        auto &fabricVertex11 = fabricVertex1->subVertices[1];
        auto fabricSubDeviceIaf11 = static_cast<FabricSubDeviceIaf *>(fabricVertex11->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());
        fabricSubDeviceIaf11->connections.clear();
        fabricSubDeviceIaf11->connections = connection11To20;
        fabricSubDeviceIaf11->guid = 0xABC;
    }

    auto &fabricVertex2 = driverHandle->fabricVertices[2];
    {
        auto fabricDeviceIaf = static_cast<FabricDeviceIaf *>(fabricVertex2->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());

        auto &fabricSubDeviceIaf0 = fabricDeviceIaf->subDeviceIafs[0];
        fabricSubDeviceIaf0->connections.clear();
        fabricSubDeviceIaf0->connections = connection20To11;
        fabricSubDeviceIaf0->guid = 0xABCD;

        auto &fabricVertex20 = fabricVertex2->subVertices[0];
        auto fabricSubDeviceIaf20 = static_cast<FabricSubDeviceIaf *>(fabricVertex20->pFabricDeviceInterfaces[FabricDeviceInterface::Type::iaf].get());
        fabricSubDeviceIaf20->connections.clear();
        fabricSubDeviceIaf20->connections = connection20To11;
        fabricSubDeviceIaf20->guid = 0xABCD;
    }

    for (auto &edge : driverHandle->fabricEdges) {
        delete edge;
    }
    driverHandle->fabricEdges.clear();
    for (auto &edge : driverHandle->fabricIndirectEdges) {
        delete edge;
    }
    driverHandle->fabricIndirectEdges.clear();
    FabricEdge::createEdgesFromVertices(driverHandle->fabricVertices, driverHandle->fabricEdges, driverHandle->fabricIndirectEdges);

    constexpr size_t rootToRootDirect = 2;
    constexpr size_t subDeviceToRootDirect = 4;          // D0.S0-D1, D0-D1.S0, D1.S1-D2, D1-D2.S0
    constexpr size_t subDeviceToSubDeviceDirect = 4 + 2; // 4 MDFI + 2 pairs of subdevices

    constexpr size_t rootToRootIndirect = 2;                    // Between D0 and D2
    constexpr size_t subDeviceToRootIndirect = (3 + 3 + 2) * 2; // D0 & D2 each has 3 further away subdevices, while D1 has 2
    constexpr size_t subDeviceToSubDeviceIndirect = 6 * 5 - 10; // 6 participating subdevices, each connect to 5 others, subtract 5 bidirectional direct edges

    EXPECT_EQ(driverHandle->fabricEdges.size(), rootToRootDirect + subDeviceToRootDirect + subDeviceToSubDeviceDirect);
    EXPECT_EQ(driverHandle->fabricIndirectEdges.size(), rootToRootIndirect + subDeviceToRootIndirect + subDeviceToSubDeviceIndirect);

    ze_fabric_vertex_handle_t vertexA = nullptr;
    ze_fabric_vertex_handle_t vertexB = nullptr;
    std::vector<ze_fabric_edge_handle_t> edges;
    ze_fabric_edge_exp_properties_t edgeProperties{};

    // Root to root (direct)
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetVerticesExp(edges[0], &vertexA, &vertexB));
    EXPECT_EQ(vertexA, fabricVertex0);
    EXPECT_EQ(vertexB, fabricVertex1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex2->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex2->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetVerticesExp(edges[0], &vertexA, &vertexB));
    EXPECT_EQ(vertexA, fabricVertex1);
    EXPECT_EQ(vertexB, fabricVertex2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 4u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    // Root to root (multi-hop)
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex2->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex2->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetVerticesExp(edges[0], &vertexA, &vertexB));
    EXPECT_EQ(vertexA, fabricVertex0);
    EXPECT_EQ(vertexB, fabricVertex2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 2u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricEdgeGetExp(fabricVertex2->toHandle(), fabricVertex0->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricEdgeGetExp(fabricVertex2->toHandle(), fabricVertex0->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetVerticesExp(edges[0], &vertexA, &vertexB));
    EXPECT_EQ(vertexA, fabricVertex2);
    EXPECT_EQ(vertexB, fabricVertex0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 2u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    // Root to subdevice (direct)
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->toHandle(), fabricVertex1->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->toHandle(), fabricVertex1->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 4u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex0->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex0->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex2->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex2->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 4u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    // Root to subdevice (multi-hop)
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "XeLink-MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex2->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex2->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 2u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex2->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex2->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "XeLink-XeLink-MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->toHandle(), fabricVertex1->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->toHandle(), fabricVertex1->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 4u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "XeLink-MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->toHandle(), fabricVertex0->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->toHandle(), fabricVertex0->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 2u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->toHandle(), fabricVertex0->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->toHandle(), fabricVertex0->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "XeLink-XeLink-MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex0->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex0->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "XeLink-MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex2->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex2->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 4u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "XeLink-MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    // Subdevice to root (direct)
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[0], fabricVertex1->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[0], fabricVertex1->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[0], fabricVertex1->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[0], fabricVertex1->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 4u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex0->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex0->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex2->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex2->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 4u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    // Subdevice to root (multi-hop)
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[0], fabricVertex2->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[0], fabricVertex2->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 2u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex1->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex1->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex2->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex2->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[0], fabricVertex0->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[0], fabricVertex0->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 2u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[1], fabricVertex1->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[1], fabricVertex1->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 4u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[1], fabricVertex0->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[1], fabricVertex0->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex0->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex0->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex2->toHandle(), &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex2->toHandle(), &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 4u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    // Subdevice to subdevice (direct)
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex0->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex0->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex2->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex2->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 4u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex0->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex0->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 0u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_UNKNOWN);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex1->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex1->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 0u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_UNKNOWN);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[1], fabricVertex2->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[1], fabricVertex2->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 0u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_UNKNOWN);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(driverHandle->fabricVertices[3]->subVertices[1], driverHandle->fabricVertices[3]->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(driverHandle->fabricVertices[3]->subVertices[1], driverHandle->fabricVertices[3]->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 0u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_UNKNOWN);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    // Subdevice to subdevice (multi-hop)
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex1->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex1->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex1->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex1->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink-MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex2->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex2->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex2->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex2->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink-XeLink-MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[0], fabricVertex1->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[0], fabricVertex1->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 4u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "XeLink-MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[0], fabricVertex0->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[0], fabricVertex0->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 2u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[0], fabricVertex0->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex2->subVertices[0], fabricVertex0->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "XeLink-XeLink-MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex2->subVertices[0], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex2->subVertices[0], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 4u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex0->subVertices[1], &count, nullptr));
    edges.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex0->subVertices[1], &count, edges.data()));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 3u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI-XeLink-MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    for (auto &edge : driverHandle->fabricEdges) {
        delete edge;
    }
    driverHandle->fabricEdges.clear();
    for (auto &edge : driverHandle->fabricIndirectEdges) {
        delete edge;
    }
    driverHandle->fabricIndirectEdges.clear();
}

} // namespace ult
} // namespace L0
