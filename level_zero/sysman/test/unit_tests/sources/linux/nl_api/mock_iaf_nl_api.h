/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/nl_api/sysman_iaf_nl_api.h"

#include <sstream>

namespace L0 {
namespace Sysman {
namespace ult {

class MockIafNlApi : public L0::Sysman::IafNlApi {
  public:
    std::vector<ze_result_t> mockroutingGenQueryReturnStatus{};
    ze_result_t mockfPortStatusQueryReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockfportPropertiesReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockdeviceEnumReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockfabricDevicePropertiesReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mocksubdevicePropertiesGetReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockportBeaconStateQueryReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockportEnableReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockportUsageEnableReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockportUsageDisableReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockportDisableReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockportStateQueryReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockremRequestReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockgetMultiPortThroughPutReturnStatus = ZE_RESULT_SUCCESS;
    bool isRepeated = false;

    ze_result_t getPorts(const std::string &devicePciPath, std::vector<L0::Sysman::IafPort> &ports) override {

        L0::Sysman::IafPort defaultPort;
        defaultPort.onSubdevice = true;
        defaultPort.portId.fabricId = testPortId.fabricId;
        defaultPort.portId.attachId = testPortId.attachId;
        defaultPort.portId.portNumber = testPortId.portNumber;
        defaultPort.model = "XeLink";
        defaultPort.maxRxSpeed.width = -1;
        defaultPort.maxRxSpeed.bitRate = -1;
        defaultPort.maxTxSpeed.width = -1;
        defaultPort.maxTxSpeed.bitRate = -1;

        if (mockPorts.size() == 0) {
            for (uint32_t i = 0; i < numPorts; i++) {
                mockPorts.push_back(defaultPort);
            }
        }

        ports = mockPorts;
        return mockGetPortsResult;
    }

    ze_result_t fPortStatusQuery(const L0::Sysman::IafPortId portId, L0::Sysman::IafPortState &state) override;
    ze_result_t getThroughput(const L0::Sysman::IafPortId portId, L0::Sysman::IafPortThroughPut &throughput) override;
    ze_result_t portStateQuery(const L0::Sysman::IafPortId portId, bool &enabled) override;
    ze_result_t portBeaconStateQuery(const L0::Sysman::IafPortId portId, bool &enabled) override;
    ze_result_t portBeaconEnable(const L0::Sysman::IafPortId portId) override;
    ze_result_t portBeaconDisable(const L0::Sysman::IafPortId portId) override;
    ze_result_t portEnable(const L0::Sysman::IafPortId portId) override;
    ze_result_t portDisable(const L0::Sysman::IafPortId portId) override;
    ze_result_t portUsageEnable(const L0::Sysman::IafPortId portId) override;
    ze_result_t portUsageDisable(const L0::Sysman::IafPortId portId) override;
    ze_result_t remRequest() override;
    ze_result_t routingGenQuery(uint32_t &start, uint32_t &end) override;
    ze_result_t deviceEnum(std::vector<uint32_t> &fabricIds) override;
    ze_result_t fabricDeviceProperties(const uint32_t fabricId, uint32_t &numSubdevices) override;
    ze_result_t subdevicePropertiesGet(const uint32_t fabricId, const uint32_t attachId, uint64_t &guid, std::vector<uint8_t> &ports) override;
    ze_result_t fportProperties(const L0::Sysman::IafPortId portId, uint64_t &neighborGuid, uint8_t &neighborPortNumber,
                                L0::Sysman::IafPortSpeed &maxRxSpeed, L0::Sysman::IafPortSpeed &maxTxSpeed,
                                L0::Sysman::IafPortSpeed &rxSpeed, L0::Sysman::IafPortSpeed &txSpeed) override;

    ze_result_t getMultiPortThroughPut(std::vector<L0::Sysman::IafPortId> &iafPortIdList, std::vector<L0::Sysman::IafThroughPutInfo> &throughput) override;
    bool validateParams = true;

    static constexpr uint32_t defaultFabricId = 0x10000000U;
    static constexpr L0::Sysman::IafPortId testPortId{defaultFabricId, 0x1U, 7};

    static const uint32_t fabricIdCount = 6;
    std::vector<uint32_t> testFabricIds;

    uint32_t portsPerSubdevice = 8U;
    uint32_t subDeviceCount = 2U;
    uint32_t numPorts = subDeviceCount * portsPerSubdevice;

    uint64_t timeStamp = 100000000UL;
    uint64_t rxCounter = 100000000UL; // Any non-zero, non-maxlongint value
    uint64_t txCounter = 200000000UL; // Any non-zero, non-maxlongint value

    zes_fabric_port_status_t fPortStatus = ZES_FABRIC_PORT_STATUS_HEALTHY;
    uint64_t constructGuid(const uint32_t fabricId, const uint32_t attachId) {
        union {
            uint32_t u32[2];
            uint64_t u64;
        } s;
        s.u32[0] = fabricId;
        s.u32[1] = attachId;
        return s.u64;
    }

    bool portEnabled = true;
    bool portBeaconing = true;

    uint32_t genStart = 0;
    uint32_t genEnd = 0;
    std::vector<L0::Sysman::IafPort> mockPorts = {};
    std::vector<L0::Sysman::IafPortId> mockIafPortIds = {};
    ze_result_t mockGetPortsResult = ZE_RESULT_SUCCESS;
    L0::Sysman::IafPortState mockPortState = {};
};

} // namespace ult
} // namespace Sysman
} // namespace L0
