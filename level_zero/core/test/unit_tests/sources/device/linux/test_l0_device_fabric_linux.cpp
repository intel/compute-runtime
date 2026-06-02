/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_fabric.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
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

static void setupDrmFabricForDevices(Mock<L0::DriverHandle> &driverHandle) {
    for (auto &l0device : driverHandle.devices) {
        auto &rootDeviceEnvironment = *l0device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0device->getRootDeviceIndex()];
        if (!rootDeviceEnvironment.osInterface) {
            auto osIface = new NEO::OSInterface();
            auto drmMock = new DrmMockResources(rootDeviceEnvironment);
            drmMock->drmFabric = std::make_unique<NEO::DrmFabricStub>();
            rootDeviceEnvironment.osInterface.reset(osIface);
            rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<NEO::Drm>(drmMock));
        }
    }
}

struct MultipleDevicesFabricFixture : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.EnableWalkerPartition.set(-1);
        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        VariableBackup<bool> mockDeviceFlagBackup(&NEO::MockDevice::createSingleDevice, false);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }

        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
        driverHandle->initialize(std::move(devices));

        setupDrmFabricForDevices(*driverHandle);
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandle>> driverHandle;
    std::unique_ptr<UltDeviceFactory> deviceFactory;

    const uint32_t numRootDevices = 2u;
    const uint32_t numSubDevices = 2u;
};

using P2pBandwidthPropertiesTest = MultipleDevicesFabricFixture;
TEST_F(P2pBandwidthPropertiesTest, GivenDirectFabricConnectionBetweenDevicesWhenQueryingBandwidthPropertiesThenCorrectPropertiesAreSet) {

    const uint32_t testLatency = 1;
    const uint32_t testBandwidth = 20;

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

TEST_F(P2pBandwidthPropertiesTest, GivenNoXeLinkFabricConnectionBetweenDevicesWhenQueryingBandwidthPropertiesThenBandwidthIsZero) {

    const uint32_t testLatency = 1;
    const uint32_t testBandwidth = 20;

    driverHandle->initializeVertexes();

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    FabricEdge testEdge{};
    testEdge.vertexA = static_cast<Device *>(device0)->fabricVertex;
    testEdge.vertexB = static_cast<Device *>(device1)->fabricVertex;
    const char *linkModel = "Dummy";
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

    // Calling with "Dummy" link
    device0->getP2PProperties(device1, &p2pProperties);
    EXPECT_EQ(0u, p2pBandwidthProps.logicalBandwidth);
    driverHandle->fabricEdges.pop_back();
}

TEST_F(P2pBandwidthPropertiesTest, GivenXeLinkAndMdfiFabricConnectionBetweenSubDevicesWhenQueryingBandwidthPropertiesThenCorrectPropertiesAreSet) {
    constexpr uint32_t xeLinkBandwidth = 3;

    driverHandle->initializeVertexes();

    uint32_t subDeviceCount = 2;
    ze_device_handle_t device0SubDevices[2];
    ze_device_handle_t device1SubDevices[2];
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->devices[0]->getSubDevices(&subDeviceCount, device0SubDevices));
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->devices[1]->getSubDevices(&subDeviceCount, device1SubDevices));
    EXPECT_NE(nullptr, device0SubDevices[0]);
    EXPECT_NE(nullptr, device0SubDevices[1]);
    EXPECT_NE(nullptr, device1SubDevices[0]);
    EXPECT_NE(nullptr, device1SubDevices[1]);

    ze_fabric_vertex_handle_t vertex00 = nullptr;
    ze_fabric_vertex_handle_t vertex01 = nullptr;
    ze_fabric_vertex_handle_t vertex10 = nullptr;
    ze_fabric_vertex_handle_t vertex11 = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[0])->getFabricVertex(&vertex00));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[1])->getFabricVertex(&vertex01));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device1SubDevices[0])->getFabricVertex(&vertex10));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device1SubDevices[1])->getFabricVertex(&vertex11));
    EXPECT_NE(nullptr, vertex00);
    EXPECT_NE(nullptr, vertex01);
    EXPECT_NE(nullptr, vertex10);
    EXPECT_NE(nullptr, vertex11);

    const char *mdfiModel = "MDFI";
    const char *xeLinkModel = "XeLink";
    const char *xeLinkMdfiModel = "XeLink-MDFI";
    const char *mdfiXeLinkModel = "MDFI-XeLink";
    const char *mdfiXeLinkMdfiModel = "MDFI-XeLink-MDFI";

    // MDFI between 00 & 01
    auto testEdgeMdfi0 = new FabricEdge;
    testEdgeMdfi0->vertexA = FabricVertex::fromHandle(vertex00);
    testEdgeMdfi0->vertexB = FabricVertex::fromHandle(vertex01);
    memcpy_s(testEdgeMdfi0->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiModel, strlen(mdfiModel));
    testEdgeMdfi0->properties.bandwidth = 0u;
    testEdgeMdfi0->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_UNKNOWN;
    testEdgeMdfi0->properties.latency = 0u;
    testEdgeMdfi0->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
    driverHandle->fabricEdges.push_back(testEdgeMdfi0);

    // MDFI between 10 & 11
    auto testEdgeMdfi1 = new FabricEdge;
    testEdgeMdfi1->vertexA = FabricVertex::fromHandle(vertex10);
    testEdgeMdfi1->vertexB = FabricVertex::fromHandle(vertex11);
    memcpy_s(testEdgeMdfi1->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiModel, strlen(mdfiModel));
    testEdgeMdfi1->properties.bandwidth = 0u;
    testEdgeMdfi1->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_UNKNOWN;
    testEdgeMdfi1->properties.latency = 0u;
    testEdgeMdfi1->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
    driverHandle->fabricEdges.push_back(testEdgeMdfi1);

    // XeLink between 01 & 10
    auto testEdgeXeLink = new FabricEdge;
    testEdgeXeLink->vertexA = FabricVertex::fromHandle(vertex01);
    testEdgeXeLink->vertexB = FabricVertex::fromHandle(vertex10);
    memcpy_s(testEdgeXeLink->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, xeLinkModel, strlen(xeLinkModel));
    testEdgeXeLink->properties.bandwidth = xeLinkBandwidth;
    testEdgeXeLink->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    testEdgeXeLink->properties.latency = 1u;
    testEdgeXeLink->properties.latencyUnit = ZE_LATENCY_UNIT_HOP;
    driverHandle->fabricEdges.push_back(testEdgeXeLink);

    // MDFI-XeLink between 00 & 10
    auto testEdgeMdfiXeLink = new FabricEdge;
    testEdgeMdfiXeLink->vertexA = FabricVertex::fromHandle(vertex00);
    testEdgeMdfiXeLink->vertexB = FabricVertex::fromHandle(vertex10);
    memcpy_s(testEdgeMdfiXeLink->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiXeLinkModel, strlen(mdfiXeLinkModel));
    testEdgeMdfiXeLink->properties.bandwidth = xeLinkBandwidth;
    testEdgeMdfiXeLink->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    testEdgeMdfiXeLink->properties.latency = std::numeric_limits<uint32_t>::max();
    testEdgeMdfiXeLink->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
    driverHandle->fabricIndirectEdges.push_back(testEdgeMdfiXeLink);

    // MDFI-XeLink-MDFI between 00 & 11
    auto testEdgeMdfiXeLinkMdfi = new FabricEdge;
    testEdgeMdfiXeLinkMdfi->vertexA = FabricVertex::fromHandle(vertex00);
    testEdgeMdfiXeLinkMdfi->vertexB = FabricVertex::fromHandle(vertex11);
    memcpy_s(testEdgeMdfiXeLinkMdfi->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiXeLinkMdfiModel, strlen(mdfiXeLinkMdfiModel));
    testEdgeMdfiXeLinkMdfi->properties.bandwidth = xeLinkBandwidth;
    testEdgeMdfiXeLinkMdfi->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    testEdgeMdfiXeLinkMdfi->properties.latency = std::numeric_limits<uint32_t>::max();
    testEdgeMdfiXeLinkMdfi->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
    driverHandle->fabricIndirectEdges.push_back(testEdgeMdfiXeLinkMdfi);

    // XeLink-MDFI between 01 & 11
    auto testEdgeXeLinkMdfi = new FabricEdge;
    testEdgeXeLinkMdfi->vertexA = FabricVertex::fromHandle(vertex01);
    testEdgeXeLinkMdfi->vertexB = FabricVertex::fromHandle(vertex11);
    memcpy_s(testEdgeXeLinkMdfi->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, xeLinkMdfiModel, strlen(xeLinkMdfiModel));
    testEdgeXeLinkMdfi->properties.bandwidth = xeLinkBandwidth;
    testEdgeXeLinkMdfi->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    testEdgeXeLinkMdfi->properties.latency = std::numeric_limits<uint32_t>::max();
    testEdgeXeLinkMdfi->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
    driverHandle->fabricIndirectEdges.push_back(testEdgeXeLinkMdfi);

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};

    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    // Check MDFI
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[0])->getP2PProperties(device0SubDevices[1], &p2pProperties));
    EXPECT_EQ(0u, p2pBandwidthProps.logicalBandwidth);
    EXPECT_EQ(0u, p2pBandwidthProps.physicalBandwidth);
    EXPECT_EQ(ZE_BANDWIDTH_UNIT_UNKNOWN, p2pBandwidthProps.bandwidthUnit);
    EXPECT_EQ(0u, p2pBandwidthProps.logicalLatency);
    EXPECT_EQ(0u, p2pBandwidthProps.physicalLatency);
    EXPECT_EQ(ZE_LATENCY_UNIT_UNKNOWN, p2pBandwidthProps.latencyUnit);

    // Check XeLink
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[1])->getP2PProperties(device1SubDevices[0], &p2pProperties));
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.logicalBandwidth);
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.physicalBandwidth);
    EXPECT_EQ(ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC, p2pBandwidthProps.bandwidthUnit);
    EXPECT_EQ(1u, p2pBandwidthProps.logicalLatency);
    EXPECT_EQ(1u, p2pBandwidthProps.physicalLatency);
    EXPECT_EQ(ZE_LATENCY_UNIT_HOP, p2pBandwidthProps.latencyUnit);

    // Check MDFI-XeLink
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[0])->getP2PProperties(device1SubDevices[0], &p2pProperties));
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.logicalBandwidth);
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.physicalBandwidth);
    EXPECT_EQ(ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC, p2pBandwidthProps.bandwidthUnit);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), p2pBandwidthProps.logicalLatency);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), p2pBandwidthProps.physicalLatency);
    EXPECT_EQ(ZE_LATENCY_UNIT_UNKNOWN, p2pBandwidthProps.latencyUnit);

    // Check MDFI-XeLink-MDFI
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[0])->getP2PProperties(device1SubDevices[1], &p2pProperties));
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.logicalBandwidth);
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.physicalBandwidth);
    EXPECT_EQ(ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC, p2pBandwidthProps.bandwidthUnit);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), p2pBandwidthProps.logicalLatency);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), p2pBandwidthProps.physicalLatency);
    EXPECT_EQ(ZE_LATENCY_UNIT_UNKNOWN, p2pBandwidthProps.latencyUnit);

    // Check XeLink-MDFI
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[1])->getP2PProperties(device1SubDevices[1], &p2pProperties));
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.logicalBandwidth);
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.physicalBandwidth);
    EXPECT_EQ(ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC, p2pBandwidthProps.bandwidthUnit);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), p2pBandwidthProps.logicalLatency);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), p2pBandwidthProps.physicalLatency);
    EXPECT_EQ(ZE_LATENCY_UNIT_UNKNOWN, p2pBandwidthProps.latencyUnit);
}

TEST_F(P2pBandwidthPropertiesTest, GivenNoDirectFabricConnectionBetweenDevicesWhenQueryingBandwidthPropertiesThenBandwidthIsZero) {

    driverHandle->initializeVertexes();

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};

    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    // By default Xelink connections are not available.
    // So getting the p2p properties without it.
    device0->getP2PProperties(device1, &p2pProperties);
    EXPECT_EQ(0u, p2pBandwidthProps.logicalBandwidth);
}

TEST_F(P2pBandwidthPropertiesTest, GivenFabricVerticesAreNotInitializedWhenQueryingBandwidthPropertiesThenFabricVerticesAreInitialized) {

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};

    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    // Calling without initialization
    device0->getP2PProperties(device1, &p2pProperties);
    EXPECT_NE(driverHandle->fabricVertices.size(), 0u);
}

TEST_F(P2pBandwidthPropertiesTest, GivenFabricVerticesAreNotAvailableForDevicesWhenQueryingBandwidthPropertiesThenBandwidthIsZero) {

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    driverHandle->initializeVertexes();

    // Check for device 0
    auto backupFabricVertex = static_cast<Device *>(device0)->fabricVertex;
    static_cast<Device *>(device0)->fabricVertex = nullptr;

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};

    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    device0->getP2PProperties(device1, &p2pProperties);
    EXPECT_EQ(p2pBandwidthProps.logicalBandwidth, 0u);
    static_cast<Device *>(device0)->fabricVertex = backupFabricVertex;

    // Check for device 1
    backupFabricVertex = static_cast<Device *>(device1)->fabricVertex;
    static_cast<Device *>(device1)->fabricVertex = nullptr;

    device0->getP2PProperties(device1, &p2pProperties);
    EXPECT_EQ(p2pBandwidthProps.logicalBandwidth, 0u);
    static_cast<Device *>(device1)->fabricVertex = backupFabricVertex;
}

template <bool p2pAccess, bool p2pAtomicAccess>
struct MultipleDevicesP2PWithXeLinkFixture : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        EXPECT_EQ(numRootDevices, executionEnvironment->rootDeviceEnvironments.size());

        auto hwInfo0 = *NEO::defaultHwInfo;
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo0);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        auto hwInfo1 = *NEO::defaultHwInfo;
        executionEnvironment->rootDeviceEnvironments[1]->setHwInfoAndInitHelpers(&hwInfo1);
        executionEnvironment->rootDeviceEnvironments[1]->initGmm();

        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[0]));
        devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[1]));

        driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
        EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

        setupDrmFabricForDevices(*driverHandle);
        driverHandle->initializeVertexes();

        uint32_t subDeviceCount = 2u;
        EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->devices[0]->getSubDevices(&subDeviceCount, device0SubDevices));
        EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->devices[1]->getSubDevices(&subDeviceCount, device1SubDevices));
        EXPECT_NE(nullptr, device0SubDevices[0]);
        EXPECT_NE(nullptr, device0SubDevices[1]);
        EXPECT_NE(nullptr, device1SubDevices[0]);
        EXPECT_NE(nullptr, device1SubDevices[1]);

        static_cast<L0::Device *>(device0SubDevices[0])->getNEODevice()->crossAccessEnabledDevices[1] = p2pAccess;
        static_cast<L0::Device *>(device0SubDevices[1])->getNEODevice()->crossAccessEnabledDevices[1] = p2pAccess;
        static_cast<L0::Device *>(device1SubDevices[0])->getNEODevice()->crossAccessEnabledDevices[0] = p2pAccess;
        static_cast<L0::Device *>(device1SubDevices[1])->getNEODevice()->crossAccessEnabledDevices[0] = p2pAccess;

        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[0])->getFabricVertex(&vertex0SubVertices[0]));
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[1])->getFabricVertex(&vertex0SubVertices[1]));
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device1SubDevices[0])->getFabricVertex(&vertex1SubVertices[0]));
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device1SubDevices[1])->getFabricVertex(&vertex1SubVertices[1]));
        EXPECT_NE(nullptr, vertex0SubVertices[0]);
        EXPECT_NE(nullptr, vertex0SubVertices[1]);
        EXPECT_NE(nullptr, vertex1SubVertices[0]);
        EXPECT_NE(nullptr, vertex1SubVertices[1]);

        const char *mdfiModel = "MDFI";
        const char *xeLinkModel = "XeLink";
        const char *xeLinkMdfiModel = "XeLink-MDFI";
        const char *mdfiXeLinkModel = "MDFI-XeLink";
        const char *mdfiXeLinkMdfiModel = "MDFI-XeLink-MDFI";
        const uint32_t xeLinkBandwidth = 16;

        // MDFI in device 0
        auto edgeMdfi0 = new FabricEdge;
        edgeMdfi0->vertexA = FabricVertex::fromHandle(vertex0SubVertices[0]);
        edgeMdfi0->vertexB = FabricVertex::fromHandle(vertex0SubVertices[1]);
        memcpy_s(edgeMdfi0->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiModel, strlen(mdfiModel));
        edgeMdfi0->properties.bandwidth = 0u;
        edgeMdfi0->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_UNKNOWN;
        edgeMdfi0->properties.latency = 0u;
        edgeMdfi0->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
        driverHandle->fabricEdges.push_back(edgeMdfi0);

        // MDFI in device 1
        auto edgeMdfi1 = new FabricEdge;
        edgeMdfi1->vertexA = FabricVertex::fromHandle(vertex1SubVertices[0]);
        edgeMdfi1->vertexB = FabricVertex::fromHandle(vertex1SubVertices[1]);
        memcpy_s(edgeMdfi1->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiModel, strlen(mdfiModel));
        edgeMdfi1->properties.bandwidth = 0u;
        edgeMdfi1->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_UNKNOWN;
        edgeMdfi1->properties.latency = 0u;
        edgeMdfi1->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
        driverHandle->fabricEdges.push_back(edgeMdfi1);

        // XeLink between 0.1 & 1.0
        auto edgeXeLink = new FabricEdge;
        edgeXeLink->vertexA = FabricVertex::fromHandle(vertex0SubVertices[1]);
        edgeXeLink->vertexB = FabricVertex::fromHandle(vertex1SubVertices[0]);
        memcpy_s(edgeXeLink->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, xeLinkModel, strlen(xeLinkModel));
        edgeXeLink->properties.bandwidth = p2pAtomicAccess ? xeLinkBandwidth : 0;
        edgeXeLink->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
        edgeXeLink->properties.latency = 1u;
        edgeXeLink->properties.latencyUnit = ZE_LATENCY_UNIT_HOP;
        driverHandle->fabricEdges.push_back(edgeXeLink);

        // MDFI-XeLink between 0.0 & 1.0
        auto edgeMdfiXeLink = new FabricEdge;
        edgeMdfiXeLink->vertexA = FabricVertex::fromHandle(vertex0SubVertices[0]);
        edgeMdfiXeLink->vertexB = FabricVertex::fromHandle(vertex1SubVertices[0]);
        memcpy_s(edgeMdfiXeLink->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiXeLinkModel, strlen(mdfiXeLinkModel));
        edgeMdfiXeLink->properties.bandwidth = p2pAtomicAccess ? xeLinkBandwidth : 0;
        edgeMdfiXeLink->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
        edgeMdfiXeLink->properties.latency = std::numeric_limits<uint32_t>::max();
        edgeMdfiXeLink->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
        driverHandle->fabricIndirectEdges.push_back(edgeMdfiXeLink);

        // MDFI-XeLink-MDFI between 0.0 & 1.1
        auto edgeMdfiXeLinkMdfi = new FabricEdge;
        edgeMdfiXeLinkMdfi->vertexA = FabricVertex::fromHandle(vertex0SubVertices[0]);
        edgeMdfiXeLinkMdfi->vertexB = FabricVertex::fromHandle(vertex1SubVertices[1]);
        memcpy_s(edgeMdfiXeLinkMdfi->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiXeLinkMdfiModel, strlen(mdfiXeLinkMdfiModel));
        edgeMdfiXeLinkMdfi->properties.bandwidth = p2pAtomicAccess ? xeLinkBandwidth : 0;
        edgeMdfiXeLinkMdfi->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
        edgeMdfiXeLinkMdfi->properties.latency = std::numeric_limits<uint32_t>::max();
        edgeMdfiXeLinkMdfi->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
        driverHandle->fabricIndirectEdges.push_back(edgeMdfiXeLinkMdfi);

        // XeLink-MDFI between 0.1 & 1.1
        auto edgeXeLinkMdfi = new FabricEdge;
        edgeXeLinkMdfi->vertexA = FabricVertex::fromHandle(vertex0SubVertices[1]);
        edgeXeLinkMdfi->vertexB = FabricVertex::fromHandle(vertex1SubVertices[1]);
        memcpy_s(edgeXeLinkMdfi->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, xeLinkMdfiModel, strlen(xeLinkMdfiModel));
        edgeXeLinkMdfi->properties.bandwidth = p2pAtomicAccess ? xeLinkBandwidth : 0;
        edgeXeLinkMdfi->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
        edgeXeLinkMdfi->properties.latency = std::numeric_limits<uint32_t>::max();
        edgeXeLinkMdfi->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
        driverHandle->fabricIndirectEdges.push_back(edgeXeLinkMdfi);
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandle>> driverHandle;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;
    ze_device_handle_t device0SubDevices[numSubDevices];
    ze_device_handle_t device1SubDevices[numSubDevices];
    ze_fabric_vertex_handle_t vertex0SubVertices[numSubDevices];
    ze_fabric_vertex_handle_t vertex1SubVertices[numSubDevices];
};

using MultipleDevicesP2PWithXeLinkDevice0Access0Atomic0Device1Access0Atomic0Test = MultipleDevicesP2PWithXeLinkFixture<false, false>;
TEST_F(MultipleDevicesP2PWithXeLinkDevice0Access0Atomic0Device1Access0Atomic0Test, WhenCallingGetP2PPropertiesWithNoDeviceHavingAccessSupportThenNoDeviceHasP2PAccessSupport) {
    auto subDevice00 = Device::fromHandle(device0SubDevices[0]);
    auto subDevice01 = Device::fromHandle(device0SubDevices[1]);
    auto subDevice10 = Device::fromHandle(device1SubDevices[0]);
    auto subDevice11 = Device::fromHandle(device1SubDevices[1]);

    ze_device_p2p_properties_t p2pProperties;

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice10, &p2pProperties));
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice01->getP2PProperties(subDevice10, &p2pProperties));
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice01->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

using MultipleDevicesP2PWithXeLinkDevice0Access1Atomic0Device1Access1Atomic0Test = MultipleDevicesP2PWithXeLinkFixture<true, false>;
TEST_F(MultipleDevicesP2PWithXeLinkDevice0Access1Atomic0Device1Access1Atomic0Test, WhenCallingGetP2PPropertiesWithBothDeviceHavingAccessSupportThenBothDevicesHaveP2PAccessSupport) {
    auto subDevice00 = Device::fromHandle(device0SubDevices[0]);
    auto subDevice01 = Device::fromHandle(device0SubDevices[1]);
    auto subDevice10 = Device::fromHandle(device1SubDevices[0]);
    auto subDevice11 = Device::fromHandle(device1SubDevices[1]);

    ze_device_p2p_properties_t p2pProperties;

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice10, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice01->getP2PProperties(subDevice10, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice01->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

using MultipleDevicesP2PWithXeLinkDevice0Access1Atomic1Device1Access1Atomic1Test = MultipleDevicesP2PWithXeLinkFixture<true, true>;
TEST_F(MultipleDevicesP2PWithXeLinkDevice0Access1Atomic1Device1Access1Atomic1Test, WhenCallingGetP2PPropertiesWithBothDevicesHavingAccessAndAtomicSupportThenCorrectSupportIsReturned) {
    auto subDevice00 = Device::fromHandle(device0SubDevices[0]);
    auto subDevice01 = Device::fromHandle(device0SubDevices[1]);
    auto subDevice10 = Device::fromHandle(device1SubDevices[0]);
    auto subDevice11 = Device::fromHandle(device1SubDevices[1]);

    ze_device_p2p_properties_t p2pProperties;

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice01, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice10, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice01->getP2PProperties(subDevice10, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice01->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice10->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

} // namespace ult
} // namespace L0
