/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/fabric_port/fabric_port_imp.h"
#include "sysman/fabric_port/os_fabric_port.h"
#include "sysman/linux/fs_access.h"

namespace L0 {

class LinuxFabricPortImp : public OsFabricPort, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getLinkType(ze_bool_t verbose, zet_fabric_link_type_t *pLinkType) override;
    ze_result_t getConfig(zet_fabric_port_config_t *pConfig) override;
    ze_result_t setConfig(const zet_fabric_port_config_t *pConfig) override;
    ze_result_t getState(zet_fabric_port_state_t *pState) override;
    ze_result_t getThroughput(zet_fabric_port_throughput_t *pThroughput) override;
    void getModel(int8_t *model) override;
    void getPortUuid(zet_fabric_port_uuid_t &portUuid) override;
    void getMaxRxSpeed(zet_fabric_port_speed_t &maxRxSpeed) override;
    void getMaxTxSpeed(zet_fabric_port_speed_t &maxTxSpeed) override;

    LinuxFabricPortImp(OsSysman *pOsSysman, uint32_t portNum);
    ~LinuxFabricPortImp() override;

  private:
    uint32_t portNum = 0;
    std::string model = "";
    zet_fabric_port_uuid_t portUuid = {};
    zet_fabric_port_speed_t maxRxSpeed = {};
    zet_fabric_port_speed_t maxTxSpeed = {};
    zet_fabric_port_config_t config = {};
};

} // namespace L0
