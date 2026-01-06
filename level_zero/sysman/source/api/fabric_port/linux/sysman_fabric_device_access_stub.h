/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/fabric_port/linux/sysman_fabric_device_access.h"

namespace L0 {
namespace Sysman {

class FabricDeviceAccessStub : public FabricDeviceAccess {
  public:
    FabricDeviceAccessStub() = delete;
    FabricDeviceAccessStub(OsSysman *pOsSysman);
    ~FabricDeviceAccessStub();

    ze_result_t getState(const zes_fabric_port_id_t portId, zes_fabric_port_state_t &state) override;
    ze_result_t getThroughput(const zes_fabric_port_id_t portId, zes_fabric_port_throughput_t &throughput) override;

    ze_result_t getPortEnabledState(const zes_fabric_port_id_t portId, bool &enabled) override;
    ze_result_t getPortBeaconState(const zes_fabric_port_id_t portId, bool &enabled) override;
    ze_result_t enablePortBeaconing(const zes_fabric_port_id_t portId) override;
    ze_result_t disablePortBeaconing(const zes_fabric_port_id_t portId) override;
    ze_result_t enable(const zes_fabric_port_id_t portId) override;
    ze_result_t disable(const zes_fabric_port_id_t portId) override;
    ze_result_t enableUsage(const zes_fabric_port_id_t portId) override;
    ze_result_t disableUsage(const zes_fabric_port_id_t portId) override;
    ze_result_t forceSweep() override;
    ze_result_t routingQuery(uint32_t &start, uint32_t &end) override;

    ze_result_t getPorts(std::vector<zes_fabric_port_id_t> &ports) override;
    void getProperties(const zes_fabric_port_id_t portId, std::string &model, bool &onSubdevice,
                       uint32_t &subdeviceId, zes_fabric_port_speed_t &maxRxSpeed, zes_fabric_port_speed_t &maxTxSpeed) override;
    ze_result_t getMultiPortThroughput(std::vector<zes_fabric_port_id_t> &portIdList, zes_fabric_port_throughput_t **pThroughput) override;
};

} // namespace Sysman
} // namespace L0
