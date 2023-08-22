/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman.h"
#include <level_zero/zes_api.h>

namespace L0 {

class OsFabricDevice {
  public:
    virtual uint32_t getNumPorts() = 0;
    virtual ze_result_t getMultiPortThroughput(std::vector<zes_fabric_port_id_t> &portIdList, zes_fabric_port_throughput_t **pThroughput) = 0;

    static OsFabricDevice *create(OsSysman *pOsSysman);

    virtual ~OsFabricDevice() = default;
};

class OsFabricPort {
  public:
    virtual ze_result_t getProperties(zes_fabric_port_properties_t *pProperties) = 0;
    virtual ze_result_t getLinkType(zes_fabric_link_type_t *pLinkType) = 0;
    virtual ze_result_t getConfig(zes_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t setConfig(const zes_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t getState(zes_fabric_port_state_t *pState) = 0;
    virtual ze_result_t getThroughput(zes_fabric_port_throughput_t *pThroughput) = 0;
    virtual ze_result_t getErrorCounters(zes_fabric_port_error_counters_t *pErrors) = 0;

    static std::unique_ptr<OsFabricPort> create(OsFabricDevice *pOsFabricDevice, uint32_t portNum);

    virtual ~OsFabricPort() = default;
};

} // namespace L0
