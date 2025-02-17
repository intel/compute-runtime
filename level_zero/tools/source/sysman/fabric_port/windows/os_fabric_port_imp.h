/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/fabric_port/fabric_port_imp.h"
#include "level_zero/tools/source/sysman/fabric_port/os_fabric_port.h"

namespace L0 {

class WddmFabricDeviceImp : public OsFabricDevice, NEO::NonCopyableAndNonMovableClass {
  public:
    uint32_t getNumPorts() override;
    ze_result_t getMultiPortThroughput(std::vector<zes_fabric_port_id_t> &portIdList, zes_fabric_port_throughput_t **pThroughput) override;

    WddmFabricDeviceImp() = delete;
    WddmFabricDeviceImp(OsSysman *pOsSysman);
    ~WddmFabricDeviceImp() override;

  private:
    uint32_t numPorts = 0;
};

class WddmFabricPortImp : public OsFabricPort, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getProperties(zes_fabric_port_properties_t *pProperties) override;
    ze_result_t getLinkType(zes_fabric_link_type_t *pLinkType) override;
    ze_result_t getConfig(zes_fabric_port_config_t *pConfig) override;
    ze_result_t setConfig(const zes_fabric_port_config_t *pConfig) override;
    ze_result_t getState(zes_fabric_port_state_t *pState) override;
    ze_result_t getThroughput(zes_fabric_port_throughput_t *pThroughput) override;
    ze_result_t getErrorCounters(zes_fabric_port_error_counters_t *pErrors) override;

    WddmFabricPortImp() = delete;
    WddmFabricPortImp(OsFabricDevice *pOsFabricDevice, uint32_t portNum);
    ~WddmFabricPortImp() override;
};

} // namespace L0
