/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/fabric/fabric.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

#include <limits>

namespace L0 {
namespace ult {

struct MultipleDevicesFabricWindowsFixture : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.EnableWalkerPartition.set(-1);
        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        VariableBackup<bool> mockDeviceFlagBackup(&NEO::MockDevice::createSingleDevice, false);

        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }

        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        for (auto i = 0u; i < numRootDevices; i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }

        driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
        driverHandle->initialize(std::move(devices));
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandle>> driverHandle;
    std::unique_ptr<UltDeviceFactory> deviceFactory;

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;
};

using P2pBandwidthPropertiesWindowsTest = MultipleDevicesFabricWindowsFixture;

TEST_F(P2pBandwidthPropertiesWindowsTest, GivenWindowsWhenFabricVerticesInitializedThenVerticesCreatedForEachDevice) {
    driverHandle->initializeVertexes();
    EXPECT_EQ(numRootDevices, driverHandle->fabricVertices.size());
    for (auto &vertex : driverHandle->fabricVertices) {
        EXPECT_NE(nullptr, vertex);
    }
}

TEST_F(P2pBandwidthPropertiesWindowsTest, GivenDirectXeLinkEdgeWhenQueryingP2pBandwidthViaExtThenCorrectValuesReturned) {
    const uint32_t testBandwidth = 20u;
    const uint32_t testLatency = 1u;

    driverHandle->initializeVertexes();

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    FabricEdge testEdge{};
    testEdge.vertexA = static_cast<Device *>(device0)->fabricVertex;
    testEdge.vertexB = static_cast<Device *>(device1)->fabricVertex;
    const char *linkModel = "XeLink";
    memcpy_s(testEdge.properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, linkModel, strlen(linkModel));
    testEdge.properties.bandwidth = testBandwidth;
    testEdge.properties.latency = testLatency;
    testEdge.properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    testEdge.properties.latencyUnit = ZE_LATENCY_UNIT_HOP;
    driverHandle->fabricEdges.push_back(&testEdge);

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};
    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_EQ(testBandwidth, p2pBandwidthProps.logicalBandwidth);
    EXPECT_EQ(testBandwidth, p2pBandwidthProps.physicalBandwidth);
    EXPECT_EQ(ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC, p2pBandwidthProps.bandwidthUnit);
    EXPECT_EQ(testLatency, p2pBandwidthProps.logicalLatency);
    EXPECT_EQ(testLatency, p2pBandwidthProps.physicalLatency);
    EXPECT_EQ(ZE_LATENCY_UNIT_HOP, p2pBandwidthProps.latencyUnit);

    driverHandle->fabricEdges.pop_back();
}

TEST_F(P2pBandwidthPropertiesWindowsTest, GivenNonXeLinkEdgeWhenQueryingP2pBandwidthViaExtThenBandwidthIsZero) {
    driverHandle->initializeVertexes();

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    FabricEdge testEdge{};
    testEdge.vertexA = static_cast<Device *>(device0)->fabricVertex;
    testEdge.vertexB = static_cast<Device *>(device1)->fabricVertex;
    const char *linkModel = "PCIe";
    memcpy_s(testEdge.properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, linkModel, strlen(linkModel));
    testEdge.properties.bandwidth = 10u;
    testEdge.properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    driverHandle->fabricEdges.push_back(&testEdge);

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};
    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_EQ(0u, p2pBandwidthProps.logicalBandwidth);

    driverHandle->fabricEdges.pop_back();
}

TEST_F(P2pBandwidthPropertiesWindowsTest, GivenFabricVertexIsNullWhenQueryingP2pBandwidthThenBandwidthIsZero) {
    driverHandle->initializeVertexes();

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    auto backupFabricVertex = static_cast<Device *>(device0)->fabricVertex;
    static_cast<Device *>(device0)->fabricVertex = nullptr;

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};
    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_EQ(0u, p2pBandwidthProps.logicalBandwidth);

    static_cast<Device *>(device0)->fabricVertex = backupFabricVertex;
}

TEST_F(P2pBandwidthPropertiesWindowsTest, GivenXeLinkConnectionWithPeerAccessWhenQueryingP2pPropertiesThenAtomicsFlagIsSet) {
    const uint32_t testBandwidth = 16u;

    driverHandle->initializeVertexes();

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    device0->getNEODevice()->crossAccessEnabledDevices[1] = true;
    device1->getNEODevice()->crossAccessEnabledDevices[0] = true;

    FabricEdge testEdge{};
    testEdge.vertexA = static_cast<Device *>(device0)->fabricVertex;
    testEdge.vertexB = static_cast<Device *>(device1)->fabricVertex;
    const char *linkModel = "XeLink";
    memcpy_s(testEdge.properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, linkModel, strlen(linkModel));
    testEdge.properties.bandwidth = testBandwidth;
    testEdge.properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    testEdge.properties.latency = 1u;
    testEdge.properties.latencyUnit = ZE_LATENCY_UNIT_HOP;
    driverHandle->fabricEdges.push_back(&testEdge);

    ze_device_p2p_properties_t p2pProperties = {};
    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    driverHandle->fabricEdges.pop_back();
}

TEST_F(P2pBandwidthPropertiesWindowsTest, GivenIndirectFabricEdgeWhenFabricEdgeGetExpCalledThenEdgeIsReturned) {
    driverHandle->initializeVertexes();

    L0::Device *device0 = driverHandle->devices[0];
    uint32_t subDeviceCount = numSubDevices;
    ze_device_handle_t subDevices[numSubDevices];
    ASSERT_EQ(ZE_RESULT_SUCCESS, device0->getSubDevices(&subDeviceCount, subDevices));

    ze_fabric_vertex_handle_t vertex0 = nullptr;
    ze_fabric_vertex_handle_t vertex1 = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(subDevices[0])->getFabricVertex(&vertex0));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(subDevices[1])->getFabricVertex(&vertex1));
    ASSERT_NE(nullptr, vertex0);
    ASSERT_NE(nullptr, vertex1);

    auto *indirectEdge = new FabricEdge;
    indirectEdge->vertexA = FabricVertex::fromHandle(vertex0);
    indirectEdge->vertexB = FabricVertex::fromHandle(vertex1);
    const char *model = "MDFI-XeLink";
    memcpy_s(indirectEdge->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, model, strlen(model));
    indirectEdge->properties.bandwidth = 10u;
    indirectEdge->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    indirectEdge->properties.latency = 2u;
    indirectEdge->properties.latencyUnit = ZE_LATENCY_UNIT_HOP;
    driverHandle->fabricIndirectEdges.push_back(indirectEdge);

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricEdgeGetExp(FabricVertex::fromHandle(vertex0), FabricVertex::fromHandle(vertex1), &count, nullptr));
    EXPECT_GE(count, 1u);

    std::vector<ze_fabric_edge_handle_t> edges(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->fabricEdgeGetExp(FabricVertex::fromHandle(vertex0), FabricVertex::fromHandle(vertex1), &count, edges.data()));
    EXPECT_GE(count, 1u);

    bool indirectEdgeFound = false;
    for (auto edge : edges) {
        if (edge == indirectEdge->toHandle()) {
            indirectEdgeFound = true;
            break;
        }
    }
    EXPECT_TRUE(indirectEdgeFound);
}

} // namespace ult
} // namespace L0
