/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/fabric_port/fabric_port.h"
#include "level_zero/tools/source/sysman/fabric_port/os_fabric_port.h"
#include <level_zero/zet_api.h>

namespace L0 {

class FabricPortImp : public FabricPort, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t fabricPortGetProperties(zet_fabric_port_properties_t *pProperties) override;
    ze_result_t fabricPortGetLinkType(ze_bool_t verbose, zet_fabric_link_type_t *pLinkType) override;
    ze_result_t fabricPortGetConfig(zet_fabric_port_config_t *pConfig) override;
    ze_result_t fabricPortSetConfig(const zet_fabric_port_config_t *pConfig) override;
    ze_result_t fabricPortGetState(zet_fabric_port_state_t *pState) override;
    ze_result_t fabricPortGetThroughput(zet_fabric_port_throughput_t *pThroughput) override;

    FabricPortImp() = default;
    FabricPortImp(OsSysman *pOsSysman, uint32_t portNum);
    ~FabricPortImp() override;
    void init();

    static uint32_t numPorts(OsSysman *pOsSysman);

  protected:
    OsFabricPort *pOsFabricPort = nullptr;

  private:
    zet_fabric_port_properties_t fabricPortProperties = {};
};

} // namespace L0
