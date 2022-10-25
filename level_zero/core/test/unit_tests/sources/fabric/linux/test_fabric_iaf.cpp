/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/fabric/fabric.h"
#include "level_zero/core/source/fabric/linux/fabric_device_iaf.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

class MockIafNlApi : public IafNlApi {
  public:
    ze_result_t fPortStatusQueryStatus = ZE_RESULT_SUCCESS;
    ze_result_t fportPropertiesStatus = ZE_RESULT_SUCCESS;
    ze_result_t subDevicePropertiesStatus = ZE_RESULT_SUCCESS;
    ze_result_t getPortsStatus = ZE_RESULT_SUCCESS;
    iaf_fport_health fPortStatusQueryHealthStatus = IAF_FPORT_HEALTH_HEALTHY;
    bool portEnumerationEnable = true;

    ze_result_t handleResponse(const uint16_t cmdOp, struct genl_info *info, void *pOutput) override { return ZE_RESULT_SUCCESS; };
    ze_result_t fPortStatusQuery(const IafPortId portId, IafPortState &state) override {
        state.healthStatus = fPortStatusQueryHealthStatus;
        return fPortStatusQueryStatus;
    };

    ze_result_t getThroughput(const IafPortId portId, IafPortThroughPut &throughput) override { return ZE_RESULT_SUCCESS; };
    ze_result_t portStateQuery(const IafPortId portId, bool &enabled) override { return ZE_RESULT_SUCCESS; };
    ze_result_t portBeaconStateQuery(const IafPortId portId, bool &enabled) override { return ZE_RESULT_SUCCESS; };
    ze_result_t portBeaconEnable(const IafPortId portId) override { return ZE_RESULT_SUCCESS; };
    ze_result_t portBeaconDisable(const IafPortId portId) override { return ZE_RESULT_SUCCESS; };
    ze_result_t portEnable(const IafPortId portId) override { return ZE_RESULT_SUCCESS; };
    ze_result_t portDisable(const IafPortId portId) override { return ZE_RESULT_SUCCESS; };
    ze_result_t portUsageEnable(const IafPortId portId) override { return ZE_RESULT_SUCCESS; };
    ze_result_t portUsageDisable(const IafPortId portId) override { return ZE_RESULT_SUCCESS; };
    ze_result_t remRequest() override { return ZE_RESULT_SUCCESS; };
    ze_result_t routingGenQuery(uint32_t &start, uint32_t &end) override { return ZE_RESULT_SUCCESS; };
    ze_result_t deviceEnum(std::vector<uint32_t> &fabricIds) override { return ZE_RESULT_SUCCESS; };
    ze_result_t fabricDeviceProperties(const uint32_t fabricId, uint32_t &numSubdevices) override { return ZE_RESULT_SUCCESS; };
    ze_result_t subdevicePropertiesGet(const uint32_t fabricId, const uint32_t attachId, uint64_t &guid, std::vector<uint8_t> &ports) override {
        return subDevicePropertiesStatus;
    };
    ze_result_t fportProperties(const IafPortId portId, uint64_t &neighborGuid, uint8_t &neighborPortNumber,
                                IafPortSpeed &maxRxSpeed, IafPortSpeed &maxTxSpeed,
                                IafPortSpeed &rxSpeed, IafPortSpeed &txSpeed) override {
        neighborPortNumber = 8;
        neighborGuid = 0xFEEDBEAD;
        maxRxSpeed.width = 4;
        maxRxSpeed.bitRate = 53125000000;
        maxTxSpeed = maxRxSpeed;
        rxSpeed = maxRxSpeed;
        txSpeed = maxRxSpeed;

        return fportPropertiesStatus;
    };
    ze_result_t getPorts(const std::string &devicePciPath, std::vector<IafPort> &ports) override {

        if (portEnumerationEnable) {
            IafPort defaultPort;
            defaultPort.onSubdevice = true;
            defaultPort.portId.fabricId = testPortId.fabricId;
            defaultPort.portId.attachId = testPortId.attachId;
            defaultPort.portId.portNumber = testPortId.portNumber;
            defaultPort.model = "XeLink";
            defaultPort.maxRxSpeed.width = 4;
            defaultPort.maxRxSpeed.bitRate = 53125000000;
            defaultPort.maxTxSpeed = defaultPort.maxRxSpeed;

            ports.push_back(defaultPort);
            defaultPort.portId.fabricId = testPortId.fabricId + 1;
            defaultPort.portId.attachId = testPortId.attachId + 1;
            defaultPort.portId.portNumber = testPortId.portNumber + 1;
            ports.push_back(defaultPort);
            defaultPort.portId.fabricId = testPortId.fabricId + 2;
            defaultPort.portId.attachId = testPortId.attachId;
            defaultPort.portId.portNumber = testPortId.portNumber + 2;
            ports.push_back(defaultPort);
        }

        return getPortsStatus;
    };

    IafPortId testPortId = {};
};

struct TestFabricIaf : public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.CreateMultipleSubDevices.set(1);
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        osInterface = new OSInterface();
        drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);
    }
    void TearDown() override {}

    DebugManagerStateRestore restorer;

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    OSInterface *osInterface = nullptr;
    NEO::MockDevice *neoDevice = nullptr;
    DrmMockResources *drmMock = nullptr;
    L0::DriverHandleImp *driverHandle = nullptr;
};

TEST_F(TestFabricIaf, GivenIafFabricAvailableWhenFabricVerticesAreCreatedThenEnumerationIsSuccessful) {

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));

    auto deviceImp = static_cast<DeviceImp *>(driverHandle->devices[0]);

    FabricSubDeviceIaf *subDeviceFabric = new FabricSubDeviceIaf(deviceImp);
    subDeviceFabric->setIafNlApi(new MockIafNlApi());
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDeviceFabric->enumerate());
    delete subDeviceFabric;

    FabricDeviceIaf *deviceFabric = new FabricDeviceIaf(deviceImp);
    deviceFabric->subDeviceIafs[0]->setIafNlApi(new MockIafNlApi());
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceFabric->enumerate());
    delete deviceFabric;
}

TEST_F(TestFabricIaf, GivenIafFabricAvailableWhenPortStatusQueryIsUnsuccessfulThenNoConnectionsAreEnumerated) {

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));

    auto deviceImp = static_cast<DeviceImp *>(driverHandle->devices[0]);

    FabricSubDeviceIaf *subDeviceFabric = new FabricSubDeviceIaf(deviceImp);
    MockIafNlApi *mockNlApi = new MockIafNlApi();
    mockNlApi->fPortStatusQueryStatus = ZE_RESULT_ERROR_UNKNOWN;
    subDeviceFabric->setIafNlApi(mockNlApi);
    subDeviceFabric->enumerate();
    EXPECT_EQ(subDeviceFabric->connections.size(), 0u);
    delete subDeviceFabric;

    subDeviceFabric = new FabricSubDeviceIaf(deviceImp);
    mockNlApi = new MockIafNlApi();
    mockNlApi->fPortStatusQueryStatus = ZE_RESULT_SUCCESS;
    mockNlApi->fPortStatusQueryHealthStatus = IAF_FPORT_HEALTH_DEGRADED;
    subDeviceFabric->setIafNlApi(mockNlApi);
    subDeviceFabric->enumerate();
    EXPECT_EQ(subDeviceFabric->connections.size(), 0u);
    delete subDeviceFabric;
}

TEST_F(TestFabricIaf, GivenIafFabricAvailableWhenPortPropertiesQueryIsUnsuccessfulThenNoConnectionsAreEnumerated) {

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));

    auto deviceImp = static_cast<DeviceImp *>(driverHandle->devices[0]);

    FabricSubDeviceIaf *subDeviceFabric = new FabricSubDeviceIaf(deviceImp);
    MockIafNlApi *mockNlApi = new MockIafNlApi();
    mockNlApi->fportPropertiesStatus = ZE_RESULT_ERROR_UNKNOWN;
    subDeviceFabric->setIafNlApi(mockNlApi);
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDeviceFabric->enumerate());
    EXPECT_EQ(subDeviceFabric->connections.size(), 0u);
    delete subDeviceFabric;
}

TEST_F(TestFabricIaf, GivenIafFabricAvailableWhenSubDevicePropertiesGetIsUnsuccessfulThenReturnError) {

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));

    auto deviceImp = static_cast<DeviceImp *>(driverHandle->devices[0]);

    FabricSubDeviceIaf *subDeviceFabric = new FabricSubDeviceIaf(deviceImp);
    MockIafNlApi *mockNlApi = new MockIafNlApi();
    mockNlApi->subDevicePropertiesStatus = ZE_RESULT_ERROR_UNKNOWN;
    subDeviceFabric->setIafNlApi(mockNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, subDeviceFabric->enumerate());
    delete subDeviceFabric;
}

TEST_F(TestFabricIaf, GivenIafFabricAvailableWhenNoPortsCanBeEnumeratedThenReturnSuccess) {

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));

    auto deviceImp = static_cast<DeviceImp *>(driverHandle->devices[0]);

    FabricSubDeviceIaf *subDeviceFabric = new FabricSubDeviceIaf(deviceImp);
    MockIafNlApi *mockNlApi = new MockIafNlApi();
    mockNlApi->portEnumerationEnable = false;
    subDeviceFabric->setIafNlApi(mockNlApi);
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDeviceFabric->enumerate());
    delete subDeviceFabric;
}

TEST_F(TestFabricIaf, GivenIafFabricAvailableWhenGetPortsReturnsErrorThenReturnError) {
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    auto deviceImp = static_cast<DeviceImp *>(driverHandle->devices[0]);

    FabricSubDeviceIaf *subDeviceFabric = new FabricSubDeviceIaf(deviceImp);
    MockIafNlApi *mockNlApi = new MockIafNlApi();
    mockNlApi->getPortsStatus = ZE_RESULT_ERROR_UNKNOWN;
    subDeviceFabric->setIafNlApi(mockNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, subDeviceFabric->enumerate());
    delete subDeviceFabric;
}

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

using FabricIafEdgeFixture = Test<MultiDeviceFixture>;
TEST_F(FabricIafEdgeFixture, GivenMultipleDevicesAndSubDevicesWhenCreatingEdgesThenEdgesCreatedAreCorrect) {

    // Setup OsInterface for Devices
    for (auto &device : driverHandle->devices) {
        auto executionEnvironment = device->getNEODevice()->getExecutionEnvironment();
        auto &rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()];
        auto osInterface = new OSInterface();
        auto drmMock = new DrmMockResources(*rootDeviceEnvironment);
        drmMock->ioctlHelper.reset(new MockIoctlHelperIafTest(*drmMock));
        rootDeviceEnvironment->osInterface.reset(osInterface);
        executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
    }

    // initialize
    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    ze_result_t res = driverHandle->fabricVertexGetExp(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    phVertices.resize(count);
    res = driverHandle->fabricVertexGetExp(&count, phVertices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    //  IAF port connection configuration
    //    Device | SubDevice | Port -- Connected to -- Device | SubDevice | Port
    //    0           0       1                           1       0           2
    //    0           0       5                           1       0           4
    //    0           1       1                           1       1           8
    //    0           1       2                           1       1           7
    //
    //    Guids:
    //    Device 0, subdevice 0 = 0xA
    //    Device 0, subdevice 1 = 0xAB
    //    Device 1, subdevice 0 = 0xABC
    //    Device 1, subdevice 1 = 0xABCD

    std::vector<FabricPortConnection> connection00To10;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(0, 0, 1);
        connection.neighbourPortNumber = 2;
        connection.neighbourGuid = 0xABC;
        connection.bandwidthInBytesPerNanoSecond = 1;
        connection.isDuplex = true;
        connection00To10.push_back(connection);
    }

    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(0, 0, 5);
        connection.neighbourPortNumber = 4;
        connection.neighbourGuid = 0xABC;
        connection.bandwidthInBytesPerNanoSecond = 1;
        connection.isDuplex = true;
        connection00To10.push_back(connection);
    }

    std::vector<FabricPortConnection> connection01To11;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(0, 1, 1);
        connection.neighbourPortNumber = 8;
        connection.neighbourGuid = 0xABCD;
        connection.bandwidthInBytesPerNanoSecond = 1;
        connection.isDuplex = true;
        connection01To11.push_back(connection);
    }

    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(0, 1, 2);
        connection.neighbourPortNumber = 7;
        connection.neighbourGuid = 0xABCD;
        connection.bandwidthInBytesPerNanoSecond = 1;
        connection.isDuplex = true;
        connection01To11.push_back(connection);
    }

    std::vector<FabricPortConnection> connection10To00;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(1, 0, 2);
        connection.neighbourPortNumber = 1;
        connection.neighbourGuid = 0xA;
        connection.bandwidthInBytesPerNanoSecond = 1;
        connection.isDuplex = true;
        connection10To00.push_back(connection);
    }
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(1, 0, 4);
        connection.neighbourPortNumber = 5;
        connection.neighbourGuid = 0xA;
        connection.bandwidthInBytesPerNanoSecond = 1;
        connection.isDuplex = true;
        connection10To00.push_back(connection);
    }

    std::vector<FabricPortConnection> connection11To01;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(1, 1, 8);
        connection.neighbourPortNumber = 1;
        connection.neighbourGuid = 0xAB;
        connection.bandwidthInBytesPerNanoSecond = 1;
        connection.isDuplex = true;
        connection11To01.push_back(connection);
    }
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(1, 1, 7);
        connection.neighbourPortNumber = 2;
        connection.neighbourGuid = 0xAB;
        connection.bandwidthInBytesPerNanoSecond = 1;
        connection.isDuplex = true;
        connection11To01.push_back(connection);
    }

    auto &fabricVertex0 = driverHandle->fabricVertices[0];
    {
        auto fabricDeviceIaf = static_cast<FabricDeviceIaf *>(fabricVertex0->pFabricDeviceInterfaces[FabricDeviceInterface::Type::Iaf].get());
        auto &fabricSubDeviceIaf0 = fabricDeviceIaf->subDeviceIafs[0];
        fabricSubDeviceIaf0->connections.clear();
        fabricSubDeviceIaf0->connections = connection00To10;
        fabricSubDeviceIaf0->guid = 0xA;

        auto &fabricSubDeviceIaf1 = fabricDeviceIaf->subDeviceIafs[1];
        fabricSubDeviceIaf1->connections.clear();
        fabricSubDeviceIaf1->connections = connection01To11;
        fabricSubDeviceIaf1->guid = 0xAB;

        //SubVertices
        auto &fabricVertex00 = fabricVertex0->subVertices[0];
        auto fabricSubDeviceIaf00 = static_cast<FabricSubDeviceIaf *>(fabricVertex00->pFabricDeviceInterfaces[FabricDeviceInterface::Type::Iaf].get());
        fabricSubDeviceIaf00->connections.clear();
        fabricSubDeviceIaf00->connections = connection00To10;
        fabricSubDeviceIaf00->guid = 0xA;

        auto &fabricVertex01 = fabricVertex0->subVertices[1];
        auto fabricSubDeviceIaf01 = static_cast<FabricSubDeviceIaf *>(fabricVertex01->pFabricDeviceInterfaces[FabricDeviceInterface::Type::Iaf].get());
        fabricSubDeviceIaf01->connections.clear();
        fabricSubDeviceIaf01->connections = connection01To11;
        fabricSubDeviceIaf01->guid = 0xAB;
    }

    auto fabricVertex1 = static_cast<FabricVertex *>(driverHandle->fabricVertices[1]);
    {
        auto fabricDeviceIaf = static_cast<FabricDeviceIaf *>(fabricVertex1->pFabricDeviceInterfaces[FabricDeviceInterface::Type::Iaf].get());
        auto &fabricSubDeviceIaf0 = fabricDeviceIaf->subDeviceIafs[0];
        fabricSubDeviceIaf0->connections.clear();
        fabricSubDeviceIaf0->connections = connection10To00;
        fabricSubDeviceIaf0->guid = 0xABC;

        auto &fabricSubDeviceIaf1 = fabricDeviceIaf->subDeviceIafs[1];
        fabricSubDeviceIaf1->connections.clear();
        fabricSubDeviceIaf1->connections = connection11To01;
        fabricSubDeviceIaf1->guid = 0xABCD;

        //SubVertices
        auto &fabricVertex00 = fabricVertex1->subVertices[0];
        auto fabricSubDeviceIaf00 = static_cast<FabricSubDeviceIaf *>(fabricVertex00->pFabricDeviceInterfaces[FabricDeviceInterface::Type::Iaf].get());
        fabricSubDeviceIaf00->connections.clear();
        fabricSubDeviceIaf00->connections = connection10To00;
        fabricSubDeviceIaf00->guid = 0xABC;

        auto &fabricVertex01 = fabricVertex1->subVertices[1];
        auto fabricSubDeviceIaf01 = static_cast<FabricSubDeviceIaf *>(fabricVertex01->pFabricDeviceInterfaces[FabricDeviceInterface::Type::Iaf].get());
        fabricSubDeviceIaf01->connections.clear();
        fabricSubDeviceIaf01->connections = connection11To01;
        fabricSubDeviceIaf01->guid = 0xABCD;
    }

    for (auto &edge : driverHandle->fabricEdges) {
        delete edge;
    }
    driverHandle->fabricEdges.clear();
    FabricEdge::createEdgesFromVertices(driverHandle->fabricVertices, driverHandle->fabricEdges);

    constexpr uint32_t root2root = 1;
    constexpr uint32_t subDevice2root = 4;          // 2 root to 2 sub-devices each
    constexpr uint32_t subDevice2SubDevice = 4 + 2; //4 MDFI (considering 4 roots with 2 sub-devices); 2 sub-device to sub-device XeLink

    EXPECT_EQ(static_cast<uint32_t>(driverHandle->fabricEdges.size()), root2root + subDevice2root + subDevice2SubDevice);

    count = 0;
    std::vector<ze_fabric_edge_handle_t> edges(30);

    // Root to Root Connection
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex0->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex0->toHandle(), &count, edges.data()));
    ze_fabric_vertex_handle_t vertexA = nullptr, vertexB = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetVerticesExp(edges[0], &vertexA, &vertexB));
    EXPECT_EQ(vertexA, fabricVertex0);
    EXPECT_EQ(vertexB, fabricVertex1);
    ze_fabric_edge_exp_properties_t edgeProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 4u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    // Root to Sub-Devices Connection
    count = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->subVertices[0], &count, nullptr));
    EXPECT_EQ(count, 1u);
    edges.clear();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->subVertices[0], &count, edges.data()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 2u);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->subVertices[1], &count, nullptr));
    EXPECT_EQ(count, 1u);
    edges.clear();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->toHandle(), fabricVertex1->subVertices[1], &count, edges.data()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 2u);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");

    // Sub-Devices to Root Connection
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[0], fabricVertex1->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    edges.clear();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[0], fabricVertex1->toHandle(), &count, edges.data()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 2u);
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);
    EXPECT_STREQ(edgeProperties.model, "XeLink");

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex1->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    edges.clear();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[1], fabricVertex1->toHandle(), &count, edges.data()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 2u);
    EXPECT_STREQ(edgeProperties.model, "XeLink");

    // Sub-Devices to Sub-Devices Connection
    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[0], fabricVertex0->subVertices[1], &count, nullptr));
    EXPECT_EQ(count, 1u);
    edges.clear();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex0->subVertices[0], fabricVertex0->subVertices[1], &count, edges.data()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 0u);
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_UNKNOWN);
    EXPECT_EQ(edgeProperties.latency, 0u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "MDFI");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex1->subVertices[1], &count, nullptr));
    EXPECT_EQ(count, 1u);
    edges.clear();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex1->subVertices[1], &count, edges.data()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_STREQ(edgeProperties.model, "MDFI");

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex0->subVertices[0], &count, nullptr));
    EXPECT_EQ(count, 1u);
    edges.clear();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[0], fabricVertex0->subVertices[0], &count, edges.data()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 2u);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.latency, 1u);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_HOP);

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex0->subVertices[1], &count, nullptr));
    EXPECT_EQ(count, 1u);
    edges.clear();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->subVertices[1], fabricVertex0->subVertices[1], &count, edges.data()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidth, 2u);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
}

TEST_F(FabricIafEdgeFixture, GivenMultipleDevicesAndSubDevicesWhenLatencyRequestIoctlFailsThenEdgeLatencyPropertiesAreUnknown) {

    // Setup OsInterface for Devices
    for (auto &device : driverHandle->devices) {
        auto executionEnvironment = device->getNEODevice()->getExecutionEnvironment();
        auto &rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()];
        auto osInterface = new OSInterface();
        auto drmMock = new DrmMockResources(*rootDeviceEnvironment);
        auto mockIoctlHelper = new MockIoctlHelperIafTest(*drmMock);
        mockIoctlHelper->mockFabricLatencyReturn = false;
        drmMock->ioctlHelper.reset(mockIoctlHelper);
        rootDeviceEnvironment->osInterface.reset(osInterface);
        executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
    }

    // initialize
    uint32_t count = 0;
    std::vector<ze_fabric_vertex_handle_t> phVertices;
    ze_result_t res = driverHandle->fabricVertexGetExp(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    phVertices.resize(count);
    res = driverHandle->fabricVertexGetExp(&count, phVertices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    //  IAF port connection configuration
    //    Device | SubDevice | Port -- Connected to -- Device | SubDevice | Port
    //    0           0       1                           1       0           2

    std::vector<FabricPortConnection> connection_00_10;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(0, 0, 1);
        connection.neighbourPortNumber = 2;
        connection.neighbourGuid = 0xABC;
        connection.bandwidthInBytesPerNanoSecond = 1;
        connection.isDuplex = true;
        connection_00_10.push_back(connection);
    }

    std::vector<FabricPortConnection> connection_10_00;
    {
        FabricPortConnection connection;
        connection.currentid = IafPortId(1, 0, 2);
        connection.neighbourPortNumber = 1;
        connection.neighbourGuid = 0xA;
        connection.bandwidthInBytesPerNanoSecond = 1;
        connection.isDuplex = true;
        connection_10_00.push_back(connection);
    }

    auto &fabricVertex0 = driverHandle->fabricVertices[0];
    {
        auto fabricDeviceIaf = static_cast<FabricDeviceIaf *>(fabricVertex0->pFabricDeviceInterfaces[FabricDeviceInterface::Type::Iaf].get());
        auto &fabricSubDeviceIaf0 = fabricDeviceIaf->subDeviceIafs[0];
        fabricSubDeviceIaf0->connections.clear();
        fabricSubDeviceIaf0->connections = connection_00_10;
        fabricSubDeviceIaf0->guid = 0xA;
    }

    auto fabricVertex1 = static_cast<FabricVertex *>(driverHandle->fabricVertices[1]);
    {
        auto fabricDeviceIaf = static_cast<FabricDeviceIaf *>(fabricVertex1->pFabricDeviceInterfaces[FabricDeviceInterface::Type::Iaf].get());
        auto &fabricSubDeviceIaf0 = fabricDeviceIaf->subDeviceIafs[0];
        fabricSubDeviceIaf0->connections.clear();
        fabricSubDeviceIaf0->connections = connection_10_00;
        fabricSubDeviceIaf0->guid = 0xABC;
    }

    for (auto &edge : driverHandle->fabricEdges) {
        delete edge;
    }
    driverHandle->fabricEdges.clear();
    FabricEdge::createEdgesFromVertices(driverHandle->fabricVertices, driverHandle->fabricEdges);
    count = 0;
    std::vector<ze_fabric_edge_handle_t> edges(30);

    // Root to Root Connection
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex0->toHandle(), &count, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetExp(fabricVertex1->toHandle(), fabricVertex0->toHandle(), &count, edges.data()));
    ze_fabric_vertex_handle_t vertexA = nullptr, vertexB = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetVerticesExp(edges[0], &vertexA, &vertexB));
    ze_fabric_edge_exp_properties_t edgeProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeFabricEdgeGetPropertiesExp(edges[0], &edgeProperties));
    EXPECT_EQ(edgeProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
    EXPECT_EQ(edgeProperties.latencyUnit, ZE_LATENCY_UNIT_UNKNOWN);
    EXPECT_STREQ(edgeProperties.model, "XeLink");
    EXPECT_EQ(edgeProperties.duplexity, ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX);
}

} // namespace ult
} // namespace L0