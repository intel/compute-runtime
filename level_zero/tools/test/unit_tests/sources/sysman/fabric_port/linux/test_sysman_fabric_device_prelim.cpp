/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fabric_port/linux/fabric_device_access_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/nl_api/mock_iaf_nl_api_prelim.h"

#include "gtest/gtest.h"

#include <limits>

extern bool sysmanUltsEnable;
namespace L0 {
namespace ult {

class PublicFabricDeviceAccessNl : public FabricDeviceAccessNl {
  public:
    using FabricDeviceAccessNl::fabricPorts;
    using FabricDeviceAccessNl::pIafNlApi;
};

class SysmanFabricDeviceFixture : public SysmanDeviceFixture {
  protected:
    MockIafNlApi *pMockIafNlApi = nullptr;
    FabricDeviceAccess *pFabricDeviceAccess = nullptr;
    zes_fabric_port_id_t testPortId = {};

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pFabricDeviceAccess = FabricDeviceAccess::create(pOsSysman);
        PublicFabricDeviceAccessNl *pPublicFabricDeviceAccessNl = reinterpret_cast<PublicFabricDeviceAccessNl *>(pFabricDeviceAccess);
        pMockIafNlApi = new MockIafNlApi;
        delete pPublicFabricDeviceAccessNl->pIafNlApi;
        pPublicFabricDeviceAccessNl->pIafNlApi = pMockIafNlApi;
        pPublicFabricDeviceAccessNl->fabricPorts.clear();

        setupDefaults();
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        delete pFabricDeviceAccess;
        SysmanDeviceFixture::TearDown();
    }

    void setupDefaults();
};

void SysmanFabricDeviceFixture::setupDefaults() {
    testPortId.attachId = pMockIafNlApi->testPortId.attachId;
    testPortId.fabricId = pMockIafNlApi->testPortId.fabricId;
    testPortId.portNumber = pMockIafNlApi->testPortId.portNumber;
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWithoutIafNlApiWhenTearDownIsCalledThenNoSegfault) {
    PublicFabricDeviceAccessNl *pPublicFabricDeviceAccessNl = reinterpret_cast<PublicFabricDeviceAccessNl *>(pFabricDeviceAccess);
    EXPECT_NE(nullptr, pPublicFabricDeviceAccessNl);
    delete pPublicFabricDeviceAccessNl->pIafNlApi;
    pPublicFabricDeviceAccessNl->pIafNlApi = nullptr;
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetPortEnabledStateIsCalledReturnsEnabledTrue) {
    bool enabled;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getPortEnabledState(testPortId, enabled));
    EXPECT_EQ(true, enabled);
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetPortBeaconStateIsCalledReturnsEnabledTrue) {
    bool enabled;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getPortBeaconState(testPortId, enabled));
    EXPECT_EQ(true, enabled);
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenEnablePortBeaconingCalledReturnsSuccess) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->enablePortBeaconing(testPortId));
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenDisablePortBeaconingCalledReturnsSuccess) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->disablePortBeaconing(testPortId));
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenEnableCalledReturnsSuccess) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->enable(testPortId));
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenDisableCalledReturnsSuccess) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->disable(testPortId));
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenEnableUsageCalledReturnsSuccess) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->enableUsage(testPortId));
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenDisableUsageCalledReturnsSuccess) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->disableUsage(testPortId));
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenForceSweepCalledReturnsSuccess) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->forceSweep());
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenRoutingQueryCalledReturnsSuccess) {
    uint32_t start = 0, end = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->routingQuery(start, end));
    EXPECT_EQ(pMockIafNlApi->genStart, start);
    EXPECT_EQ(pMockIafNlApi->genEnd, end);
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetPropertiesCalledThenAllPropertiesAreReturned) {
    PublicFabricDeviceAccessNl *pPublicFabricDeviceAccessNl = reinterpret_cast<PublicFabricDeviceAccessNl *>(pFabricDeviceAccess);

    struct Port p;
    pPublicFabricDeviceAccessNl->fabricPorts.clear();

    // First port, wrong attachId
    p.portId.fabricId = pMockIafNlApi->testPortId.fabricId;
    p.portId.attachId = 0U;
    p.portId.portNumber = pMockIafNlApi->testPortId.portNumber;
    p.onSubdevice = false;
    p.model = std::string("wrongPort");
    p.maxRxSpeed.bitRate = 1L;
    p.maxRxSpeed.width = 1;
    p.maxTxSpeed.bitRate = 1L;
    p.maxTxSpeed.width = 1;
    pPublicFabricDeviceAccessNl->fabricPorts.push_back(p);

    // Second port, wrong portNumber
    p.portId.fabricId = pMockIafNlApi->testPortId.fabricId;
    p.portId.attachId = pMockIafNlApi->testPortId.attachId;
    p.portId.portNumber = 0U;
    p.onSubdevice = false;
    p.model = std::string("wrongPort");
    p.maxRxSpeed.bitRate = 2L;
    p.maxRxSpeed.width = 2;
    p.maxTxSpeed.bitRate = 2L;
    p.maxTxSpeed.width = 2;
    pPublicFabricDeviceAccessNl->fabricPorts.push_back(p);

    // Third port, is a match
    p.portId.fabricId = pMockIafNlApi->testPortId.fabricId;
    p.portId.attachId = pMockIafNlApi->testPortId.attachId;
    p.portId.portNumber = pMockIafNlApi->testPortId.portNumber;
    p.onSubdevice = true;
    p.model = std::string("rightPort");
    p.maxRxSpeed.bitRate = 3L;
    p.maxRxSpeed.width = 3;
    p.maxTxSpeed.bitRate = 3L;
    p.maxTxSpeed.width = 3;
    pPublicFabricDeviceAccessNl->fabricPorts.push_back(p);

    std::string model = std::string("");
    bool onSubdevice = false;
    uint32_t subdeviceId = 0U;
    zes_fabric_port_speed_t maxRxSpeed = {.bitRate = -1L, .width = -1};
    zes_fabric_port_speed_t maxTxSpeed = {.bitRate = -1L, .width = -1};
    pFabricDeviceAccess->getProperties(testPortId, model, onSubdevice, subdeviceId, maxRxSpeed, maxTxSpeed);
    EXPECT_EQ(p.model, model);
    EXPECT_EQ(p.onSubdevice, onSubdevice);
    EXPECT_EQ(p.portId.attachId, subdeviceId);
    EXPECT_EQ(p.maxRxSpeed.bitRate, maxRxSpeed.bitRate);
    EXPECT_EQ(p.maxRxSpeed.width, maxRxSpeed.width);
    EXPECT_EQ(p.maxTxSpeed.bitRate, maxTxSpeed.bitRate);
    EXPECT_EQ(p.maxTxSpeed.width, maxTxSpeed.width);
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetThroughputCalledReturnsThroughputValues) {
    zes_fabric_port_throughput_t throughput = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getThroughput(testPortId, throughput));
    EXPECT_EQ(throughput.rxCounter, pMockIafNlApi->rxCounter);
    EXPECT_EQ(throughput.txCounter, pMockIafNlApi->txCounter);
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetStateIsCalledAndfPortStatusIsHealthyReturnsSuccess) {
    zes_fabric_port_state_t state1 = {};
    state1.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
    state1.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state1));
    EXPECT_EQ(state1.stype, ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES);
    EXPECT_EQ(state1.pNext, nullptr);
    EXPECT_EQ(state1.status, ZES_FABRIC_PORT_STATUS_HEALTHY);
    EXPECT_EQ(state1.qualityIssues, 0U);
    EXPECT_EQ(state1.failureReasons, 0U);
    EXPECT_EQ(state1.remotePortId.fabricId, pMockIafNlApi->testPortId.fabricId);
    EXPECT_EQ(state1.remotePortId.attachId, pMockIafNlApi->testPortId.attachId);
    EXPECT_EQ(state1.remotePortId.portNumber, pMockIafNlApi->testPortId.portNumber + 1);
    EXPECT_EQ(state1.rxSpeed.bitRate, -1);
    EXPECT_EQ(state1.rxSpeed.width, -1);
    EXPECT_EQ(state1.txSpeed.bitRate, -1);
    EXPECT_EQ(state1.txSpeed.width, -1);

    zes_fabric_port_state_t state2 = {};
    state2.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
    state2.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state2));
    EXPECT_EQ(state2.stype, ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES);
    EXPECT_EQ(state2.pNext, nullptr);
    EXPECT_EQ(state2.status, ZES_FABRIC_PORT_STATUS_HEALTHY);
    EXPECT_EQ(state2.qualityIssues, 0U);
    EXPECT_EQ(state2.failureReasons, 0U);
    EXPECT_EQ(state2.remotePortId.fabricId, pMockIafNlApi->testPortId.fabricId);
    EXPECT_EQ(state2.remotePortId.attachId, pMockIafNlApi->testPortId.attachId);
    EXPECT_EQ(state2.remotePortId.portNumber, pMockIafNlApi->testPortId.portNumber + 1);
    EXPECT_EQ(state2.rxSpeed.bitRate, -1);
    EXPECT_EQ(state2.rxSpeed.width, -1);
    EXPECT_EQ(state2.txSpeed.bitRate, -1);
    EXPECT_EQ(state2.txSpeed.width, -1);
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetStateIsCalledAndfPortStatusIsDegradedReturnsSuccess) {
    zes_fabric_port_state_t state = {};
    state.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
    state.pNext = nullptr;
    pMockIafNlApi->fPortStatus = ZES_FABRIC_PORT_STATUS_DEGRADED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
    EXPECT_EQ(state.stype, ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES);
    EXPECT_EQ(state.pNext, nullptr);
    EXPECT_EQ(state.status, ZES_FABRIC_PORT_STATUS_DEGRADED);
    EXPECT_EQ(state.qualityIssues, 0U);
    EXPECT_EQ(state.failureReasons, 0U);
    EXPECT_EQ(state.remotePortId.fabricId, pMockIafNlApi->testPortId.fabricId);
    EXPECT_EQ(state.remotePortId.attachId, pMockIafNlApi->testPortId.attachId);
    EXPECT_EQ(state.remotePortId.portNumber, pMockIafNlApi->testPortId.portNumber + 1);
    EXPECT_EQ(state.rxSpeed.bitRate, -1);
    EXPECT_EQ(state.rxSpeed.width, -1);
    EXPECT_EQ(state.txSpeed.bitRate, -1);
    EXPECT_EQ(state.txSpeed.width, -1);
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetStateIsCalledAndfPortStatusIsFailedReturnsSuccess) {
    zes_fabric_port_state_t state = {};
    state.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
    state.pNext = nullptr;
    pMockIafNlApi->fPortStatus = ZES_FABRIC_PORT_STATUS_FAILED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
    EXPECT_EQ(state.stype, ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES);
    EXPECT_EQ(state.pNext, nullptr);
    EXPECT_EQ(state.status, ZES_FABRIC_PORT_STATUS_FAILED);
    EXPECT_EQ(state.qualityIssues, 0U);
    EXPECT_EQ(state.failureReasons, 0U);
    EXPECT_EQ(state.remotePortId.fabricId, pMockIafNlApi->testPortId.fabricId);
    EXPECT_EQ(state.remotePortId.attachId, pMockIafNlApi->testPortId.attachId);
    EXPECT_EQ(state.remotePortId.portNumber, pMockIafNlApi->testPortId.portNumber + 1);
    EXPECT_EQ(state.rxSpeed.bitRate, -1);
    EXPECT_EQ(state.rxSpeed.width, -1);
    EXPECT_EQ(state.txSpeed.bitRate, -1);
    EXPECT_EQ(state.txSpeed.width, -1);
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetStateIsCalledAndfPortStatusIsDisabledReturnsSuccess) {
    zes_fabric_port_state_t state = {};
    state.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
    state.pNext = nullptr;
    pMockIafNlApi->fPortStatus = ZES_FABRIC_PORT_STATUS_DISABLED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
    EXPECT_EQ(state.stype, ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES);
    EXPECT_EQ(state.pNext, nullptr);
    EXPECT_EQ(state.status, ZES_FABRIC_PORT_STATUS_DISABLED);
    EXPECT_EQ(state.qualityIssues, 0U);
    EXPECT_EQ(state.failureReasons, 0U);
    EXPECT_EQ(state.rxSpeed.bitRate, -1);
    EXPECT_EQ(state.rxSpeed.width, -1);
    EXPECT_EQ(state.txSpeed.bitRate, -1);
    EXPECT_EQ(state.txSpeed.width, -1);
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetStateIsCalledAndfPortStatusIsUnknownReturnsSuccess) {
    zes_fabric_port_state_t state = {};
    state.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
    state.pNext = nullptr;
    pMockIafNlApi->fPortStatus = ZES_FABRIC_PORT_STATUS_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
    EXPECT_EQ(state.stype, ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES);
    EXPECT_EQ(state.pNext, nullptr);
    EXPECT_EQ(state.status, ZES_FABRIC_PORT_STATUS_UNKNOWN);
    EXPECT_EQ(state.qualityIssues, 0U);
    EXPECT_EQ(state.failureReasons, 0U);
    EXPECT_EQ(state.remotePortId.fabricId, 0U);
    EXPECT_EQ(state.remotePortId.attachId, 0U);
    EXPECT_EQ(state.remotePortId.portNumber, 0U);
    EXPECT_EQ(state.rxSpeed.bitRate, -1);
    EXPECT_EQ(state.rxSpeed.width, -1);
    EXPECT_EQ(state.txSpeed.bitRate, -1);
    EXPECT_EQ(state.txSpeed.width, -1);
}

TEST_F(SysmanFabricDeviceFixture, GivenPortStatusFailedWhenGetStateIsCalledThenVerifyFailureReasonsAreAsExpected) {
    zes_fabric_port_state_t state = {};
    state.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
    state.pNext = nullptr;
    pMockIafNlApi->fPortStatus = ZES_FABRIC_PORT_STATUS_FAILED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
    EXPECT_EQ(state.status, ZES_FABRIC_PORT_STATUS_FAILED);
    EXPECT_EQ(state.failureReasons, 0U);

    pMockIafNlApi->mockPortState.failed = 1;
    pMockIafNlApi->mockPortState.isolated = 1;
    pMockIafNlApi->mockPortState.linkDown = 1;
    pMockIafNlApi->mockPortState.didNotTrain = 1;
    pMockIafNlApi->mockPortState.flapping = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
    EXPECT_EQ(ZES_FABRIC_PORT_FAILURE_FLAG_FAILED, state.failureReasons & ZES_FABRIC_PORT_FAILURE_FLAG_FAILED);
    EXPECT_EQ(ZES_FABRIC_PORT_FAILURE_FLAG_TRAINING_TIMEOUT, state.failureReasons & ZES_FABRIC_PORT_FAILURE_FLAG_TRAINING_TIMEOUT);
    EXPECT_EQ(ZES_FABRIC_PORT_FAILURE_FLAG_FLAPPING, state.failureReasons & ZES_FABRIC_PORT_FAILURE_FLAG_FLAPPING);

    pMockIafNlApi->mockPortState.failed = 1;
    pMockIafNlApi->mockPortState.isolated = 0;
    pMockIafNlApi->mockPortState.linkDown = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
    EXPECT_EQ(ZES_FABRIC_PORT_FAILURE_FLAG_FAILED, state.failureReasons & ZES_FABRIC_PORT_FAILURE_FLAG_FAILED);

    pMockIafNlApi->mockPortState.failed = 0;
    pMockIafNlApi->mockPortState.isolated = 1;
    pMockIafNlApi->mockPortState.linkDown = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
    EXPECT_EQ(ZES_FABRIC_PORT_FAILURE_FLAG_FAILED, state.failureReasons & ZES_FABRIC_PORT_FAILURE_FLAG_FAILED);

    pMockIafNlApi->mockPortState.failed = 0;
    pMockIafNlApi->mockPortState.isolated = 0;
    pMockIafNlApi->mockPortState.linkDown = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
    EXPECT_EQ(ZES_FABRIC_PORT_FAILURE_FLAG_FAILED, state.failureReasons & ZES_FABRIC_PORT_FAILURE_FLAG_FAILED);
}

TEST_F(SysmanFabricDeviceFixture, GivenPortStatusDegradedWhenGetStateIsCalledThenVerifyQualityIssuesAreAsExpected) {
    zes_fabric_port_state_t state = {};
    state.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
    state.pNext = nullptr;
    pMockIafNlApi->fPortStatus = ZES_FABRIC_PORT_STATUS_DEGRADED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
    EXPECT_EQ(state.status, ZES_FABRIC_PORT_STATUS_DEGRADED);
    EXPECT_EQ(state.failureReasons, 0U);

    pMockIafNlApi->mockPortState.lqi = 1;
    pMockIafNlApi->mockPortState.lwd = 1;
    pMockIafNlApi->mockPortState.rate = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
    EXPECT_EQ(ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_LINK_ERRORS, state.qualityIssues & ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_LINK_ERRORS);
    EXPECT_EQ(ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_SPEED, state.qualityIssues & ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_SPEED);

    pMockIafNlApi->mockPortState.lwd = 0;
    pMockIafNlApi->mockPortState.rate = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
    EXPECT_EQ(ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_SPEED, state.qualityIssues & ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_SPEED);
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetStateIsCalledWhenFPortStatusQueryFailsReturnsError) {
    pMockIafNlApi->mockfPortStatusQueryReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fabric_port_state_t state;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFabricDeviceAccess->getState(testPortId, state));
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetStateIsCalledWhenFPortPropertiesQueryFailsReturnsError) {
    pMockIafNlApi->mockfportPropertiesReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fabric_port_state_t state;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFabricDeviceAccess->getState(testPortId, state));
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetStateIsCalledWhenDeviceEnumFailsThenGetStateReturnsSuccess) {
    pMockIafNlApi->mockdeviceEnumReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fabric_port_state_t state;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetStateIsCalledWhenFabricDevicePropertiesFailsThenGetStateReturnsSuccess) {
    pMockIafNlApi->mockfabricDevicePropertiesReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fabric_port_state_t state;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetStateIsCalledWhenSubDevicePropertiesGetFailsThenGetStateReturnsSuccess) {
    pMockIafNlApi->mocksubdevicePropertiesGetReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fabric_port_state_t state;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFabricDeviceAccess->getState(testPortId, state));
}

TEST_F(SysmanFabricDeviceFixture, GivenFabricDeviceAccessNlWhenGetPortsCalledWithNoPortsAndInitMyPortGetNumSubDevicesFailsReturnsError) {
    std::vector<zes_fabric_port_id_t> ports;
    pMockIafNlApi->mockGetPortsResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFabricDeviceAccess->getPorts(ports));
}

} // namespace ult
} // namespace L0
