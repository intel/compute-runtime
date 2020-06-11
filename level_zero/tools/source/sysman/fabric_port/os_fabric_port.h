/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman.h"
#include <level_zero/zet_api.h>

namespace L0 {

class OsFabricPort {
  public:
    virtual ze_result_t getLinkType(ze_bool_t verbose, zet_fabric_link_type_t *pLinkType) = 0;
    virtual ze_result_t getConfig(zet_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t setConfig(const zet_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t getState(zet_fabric_port_state_t *pState) = 0;
    virtual ze_result_t getThroughput(zet_fabric_port_throughput_t *pThroughput) = 0;
    virtual void getModel(int8_t *model) = 0;
    virtual void getPortUuid(zet_fabric_port_uuid_t &portUuid) = 0;
    virtual void getMaxRxSpeed(zet_fabric_port_speed_t &maxRxSpeed) = 0;
    virtual void getMaxTxSpeed(zet_fabric_port_speed_t &maxTxSpeed) = 0;

    static uint32_t numPorts(OsSysman *pOsSysman);
    static OsFabricPort *create(OsSysman *pOsSysman, uint32_t portNum);

    virtual ~OsFabricPort() = default;
};

} // namespace L0
