/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/zes_api.h"

#include <string>
#include <vector>

namespace L0 {
namespace Sysman {

struct OsSysman;

class FabricDeviceAccess : NEO::NonCopyableAndNonMovableClass {
  public:
    virtual ze_result_t getState(const zes_fabric_port_id_t portId, zes_fabric_port_state_t &state) = 0;
    virtual ze_result_t getThroughput(const zes_fabric_port_id_t portId, zes_fabric_port_throughput_t &throughput) = 0;

    virtual ze_result_t getPortEnabledState(const zes_fabric_port_id_t portId, bool &enabled) = 0;
    virtual ze_result_t getPortBeaconState(const zes_fabric_port_id_t portId, bool &enabled) = 0;
    virtual ze_result_t enablePortBeaconing(const zes_fabric_port_id_t portId) = 0;
    virtual ze_result_t disablePortBeaconing(const zes_fabric_port_id_t portId) = 0;
    virtual ze_result_t enable(const zes_fabric_port_id_t portId) = 0;
    virtual ze_result_t disable(const zes_fabric_port_id_t portId) = 0;
    virtual ze_result_t enableUsage(const zes_fabric_port_id_t portId) = 0;
    virtual ze_result_t disableUsage(const zes_fabric_port_id_t portId) = 0;
    virtual ze_result_t forceSweep() = 0;
    virtual ze_result_t routingQuery(uint32_t &start, uint32_t &end) = 0;

    virtual ze_result_t getPorts(std::vector<zes_fabric_port_id_t> &ports) = 0;
    virtual void getProperties(const zes_fabric_port_id_t portId, std::string &model, bool &onSubdevice,
                               uint32_t &subdeviceId, zes_fabric_port_speed_t &maxRxSpeed, zes_fabric_port_speed_t &maxTxSpeed) = 0;
    virtual ze_result_t getMultiPortThroughput(std::vector<zes_fabric_port_id_t> &portIdList, zes_fabric_port_throughput_t **pThroughput) = 0;

    virtual ~FabricDeviceAccess() = default;

    static FabricDeviceAccess *create(OsSysman *pOsSysman);
};

} // namespace Sysman
} // namespace L0
