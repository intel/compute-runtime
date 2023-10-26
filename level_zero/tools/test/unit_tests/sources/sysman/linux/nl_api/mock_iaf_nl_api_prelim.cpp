/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_iaf_nl_api_prelim.h"

#include "level_zero/tools/source/sysman/linux/nl_api/iaf_nl_api.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

ze_result_t MockIafNlApi::fPortStatusQuery(const IafPortId portId, IafPortState &state) {
    if (mockfPortStatusQueryReturnStatus != ZE_RESULT_SUCCESS) {
        return mockfPortStatusQueryReturnStatus;
    }

    if (validateParams) {
        EXPECT_EQ(portId.fabricId, testPortId.fabricId);
        EXPECT_EQ(portId.attachId, testPortId.attachId);
        EXPECT_EQ(portId.portNumber, testPortId.portNumber);
    }

    state = mockPortState;

    state.healthStatus = _IAF_FPORT_HEALTH_COUNT;
    switch (fPortStatus) {
    case ZES_FABRIC_PORT_STATUS_HEALTHY:
        state.healthStatus = IAF_FPORT_HEALTH_HEALTHY;
        break;
    case ZES_FABRIC_PORT_STATUS_DEGRADED:
        state.healthStatus = IAF_FPORT_HEALTH_DEGRADED;
        break;
    case ZES_FABRIC_PORT_STATUS_FAILED:
        state.healthStatus = IAF_FPORT_HEALTH_FAILED;
        break;
    case ZES_FABRIC_PORT_STATUS_DISABLED:
        state.healthStatus = IAF_FPORT_HEALTH_OFF;
        break;
    default:
        break;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::getThroughput(const IafPortId portId, IafPortThroughPut &throughput) {
    if (validateParams) {
        EXPECT_EQ(portId.fabricId, testPortId.fabricId);
        EXPECT_EQ(portId.attachId, testPortId.attachId);
        EXPECT_EQ(portId.portNumber, testPortId.portNumber);
    }

    // Timestamp is not provided by IafNlApi
    throughput.rxCounter = rxCounter;
    throughput.txCounter = txCounter;

    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::getMultiPortThroughPut(std::vector<IafPortId> &iafPortIdList, std::vector<IafThroughPutInfo> &throughputList) {
    if (mockgetMultiPortThroughPutReturnStatus != ZE_RESULT_SUCCESS) {
        return mockgetMultiPortThroughPutReturnStatus;
    }

    throughputList.resize(iafPortIdList.size());
    for (uint64_t i = 0; i < iafPortIdList.size(); i++) {
        throughputList[i].iafThroughput.rxCounter = rxCounter;
        throughputList[i].iafThroughput.txCounter = txCounter;
        if (mockIafPortIds.empty()) {
            throughputList[i].iafPortId.portNumber = testPortId.portNumber;
            throughputList[i].iafPortId.attachId = testPortId.attachId;
            throughputList[i].iafPortId.fabricId = testPortId.fabricId;
            continue;
        }
        throughputList[i].iafPortId.portNumber = mockIafPortIds[i].portNumber;
        throughputList[i].iafPortId.attachId = mockIafPortIds[i].attachId;
        throughputList[i].iafPortId.fabricId = mockIafPortIds[i].fabricId;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::portStateQuery(const IafPortId portId, bool &enabled) {
    if (mockportStateQueryReturnStatus != ZE_RESULT_SUCCESS) {
        return mockportStateQueryReturnStatus;
    }

    if (validateParams) {
        EXPECT_EQ(portId.fabricId, testPortId.fabricId);
        EXPECT_EQ(portId.attachId, testPortId.attachId);
        EXPECT_EQ(portId.portNumber, testPortId.portNumber);
    }

    enabled = portEnabled;

    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::portBeaconStateQuery(const IafPortId portId, bool &enabled) {
    if (mockportBeaconStateQueryReturnStatus != ZE_RESULT_SUCCESS) {
        return mockportBeaconStateQueryReturnStatus;
    }

    if (validateParams) {
        EXPECT_EQ(portId.fabricId, testPortId.fabricId);
        EXPECT_EQ(portId.attachId, testPortId.attachId);
        EXPECT_EQ(portId.portNumber, testPortId.portNumber);
    }

    enabled = portBeaconing;

    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::portBeaconEnable(const IafPortId portId) {
    if (validateParams) {
        EXPECT_EQ(portId.fabricId, testPortId.fabricId);
        EXPECT_EQ(portId.attachId, testPortId.attachId);
        EXPECT_EQ(portId.portNumber, testPortId.portNumber);
    }

    portBeaconing = true;

    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::portBeaconDisable(const IafPortId portId) {
    if (validateParams) {
        EXPECT_EQ(portId.fabricId, testPortId.fabricId);
        EXPECT_EQ(portId.attachId, testPortId.attachId);
        EXPECT_EQ(portId.portNumber, testPortId.portNumber);
    }

    portBeaconing = false;

    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::portEnable(const IafPortId portId) {
    if (mockportEnableReturnStatus != ZE_RESULT_SUCCESS) {
        return mockportEnableReturnStatus;
    }

    if (validateParams) {
        EXPECT_EQ(portId.fabricId, testPortId.fabricId);
        EXPECT_EQ(portId.attachId, testPortId.attachId);
        EXPECT_EQ(portId.portNumber, testPortId.portNumber);
    }

    portEnabled = true;

    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::portDisable(const IafPortId portId) {
    if (mockportDisableReturnStatus != ZE_RESULT_SUCCESS) {
        return mockportDisableReturnStatus;
    }

    if (validateParams) {
        EXPECT_EQ(portId.fabricId, testPortId.fabricId);
        EXPECT_EQ(portId.attachId, testPortId.attachId);
        EXPECT_EQ(portId.portNumber, testPortId.portNumber);
    }

    portEnabled = false;

    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::portUsageEnable(const IafPortId portId) {
    if (mockportUsageEnableReturnStatus != ZE_RESULT_SUCCESS) {
        return mockportUsageEnableReturnStatus;
    }

    if (validateParams) {
        EXPECT_EQ(portId.fabricId, testPortId.fabricId);
        EXPECT_EQ(portId.attachId, testPortId.attachId);
        EXPECT_EQ(portId.portNumber, testPortId.portNumber);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::portUsageDisable(const IafPortId portId) {
    if (mockportUsageDisableReturnStatus != ZE_RESULT_SUCCESS) {
        return mockportUsageDisableReturnStatus;
    }

    if (validateParams) {
        EXPECT_EQ(portId.fabricId, testPortId.fabricId);
        EXPECT_EQ(portId.attachId, testPortId.attachId);
        EXPECT_EQ(portId.portNumber, testPortId.portNumber);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::remRequest() {
    if (mockremRequestReturnStatus != ZE_RESULT_SUCCESS) {
        return mockremRequestReturnStatus;
    }

    genStart += 5;
    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::routingGenQuery(uint32_t &start, uint32_t &end) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    if (!mockroutingGenQueryReturnStatus.empty()) {
        result = mockroutingGenQueryReturnStatus.front();
        if (isRepeated != true || result == ZE_RESULT_SUCCESS) {
            mockroutingGenQueryReturnStatus.erase(mockroutingGenQueryReturnStatus.begin());
        }
    }

    start = genStart;
    end = ++genEnd;
    return result;
}

ze_result_t MockIafNlApi::deviceEnum(std::vector<uint32_t> &fabricIds) {
    if (mockdeviceEnumReturnStatus != ZE_RESULT_SUCCESS) {
        return mockdeviceEnumReturnStatus;
    }

    fabricIds.clear();
    for (auto i = 0U; i < fabricIdCount; i++) {
        fabricIds.push_back(testPortId.fabricId + i);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::fabricDeviceProperties(const uint32_t fabricId, uint32_t &numSubdevices) {
    if (mockfabricDevicePropertiesReturnStatus != ZE_RESULT_SUCCESS) {
        return mockfabricDevicePropertiesReturnStatus;
    }

    if (validateParams) {
        EXPECT_NE(0U, fabricId);
    }

    numSubdevices = subDeviceCount;
    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::subdevicePropertiesGet(const uint32_t fabricId, const uint32_t attachId, uint64_t &guid, std::vector<uint8_t> &ports) {
    if (mocksubdevicePropertiesGetReturnStatus != ZE_RESULT_SUCCESS) {
        return mocksubdevicePropertiesGetReturnStatus;
    }

    if (validateParams) {
        EXPECT_NE(0U, fabricId);
        EXPECT_LT(attachId, 2U);
    }

    ports.clear();
    for (auto i = 0U; i < 8U; i++) {
        ports.push_back(i + 1);
    }

    guid = constructGuid(fabricId, attachId);

    return ZE_RESULT_SUCCESS;
}

ze_result_t MockIafNlApi::fportProperties(const IafPortId portId, uint64_t &neighborGuid, uint8_t &neighborPortNumber,
                                          IafPortSpeed &maxRxSpeed, IafPortSpeed &maxTxSpeed,
                                          IafPortSpeed &rxSpeed, IafPortSpeed &txSpeed) {

    if (mockfportPropertiesReturnStatus != ZE_RESULT_SUCCESS) {
        return mockfportPropertiesReturnStatus;
    }

    if (validateParams) {
        EXPECT_EQ(portId.fabricId, testPortId.fabricId);
        EXPECT_EQ(portId.attachId, testPortId.attachId);
        EXPECT_EQ(portId.portNumber, testPortId.portNumber);
    }

    neighborGuid = constructGuid(portId.fabricId, portId.attachId);
    neighborPortNumber = portId.portNumber + 1;
    maxRxSpeed.bitRate = -1;
    maxRxSpeed.width = -1;
    maxTxSpeed.bitRate = -1;
    maxTxSpeed.width = -1;
    rxSpeed.bitRate = -1;
    rxSpeed.width = -1;
    txSpeed.bitRate = -1;
    txSpeed.width = -1;

    return ZE_RESULT_SUCCESS;
}

} // namespace ult
} // namespace L0
