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
#include "level_zero/tools/source/sysman/linux/fs_access.h"

namespace L0 {

class LinuxFabricDeviceImp : public OsFabricDevice, NEO::NonCopyableAndNonMovableClass {
  public:
    uint32_t getNumPorts() override;
    ze_result_t getMultiPortThroughput(std::vector<zes_fabric_port_id_t> &portIdList, zes_fabric_port_throughput_t **pThroughput) override;

    LinuxFabricDeviceImp() = delete;
    LinuxFabricDeviceImp(OsSysman *pOsSysman);
    ~LinuxFabricDeviceImp() override;

  private:
    uint32_t numPorts = 0;
};

class LinuxFabricPortImp : public OsFabricPort, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getProperties(zes_fabric_port_properties_t *pProperties) override;
    ze_result_t getLinkType(zes_fabric_link_type_t *pLinkType) override;
    ze_result_t getConfig(zes_fabric_port_config_t *pConfig) override;
    ze_result_t setConfig(const zes_fabric_port_config_t *pConfig) override;
    ze_result_t getState(zes_fabric_port_state_t *pState) override;
    ze_result_t getThroughput(zes_fabric_port_throughput_t *pThroughput) override;
    ze_result_t getErrorCounters(zes_fabric_port_error_counters_t *pErrors) override;

    LinuxFabricPortImp() = delete;
    LinuxFabricPortImp(OsFabricDevice *pOsFabricDevice, uint32_t portNum);
    ~LinuxFabricPortImp() override;

  private:
    uint32_t portNum = 0;
    std::string model = "";
    zes_fabric_port_id_t portId = {};
    zes_fabric_port_speed_t maxRxSpeed = {};
    zes_fabric_port_speed_t maxTxSpeed = {};
    zes_fabric_port_config_t config = {};
};

} // namespace L0
