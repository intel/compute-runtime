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

#include <string>

namespace L0 {

class WddmFabricDeviceImp : public OsFabricDevice, NEO::NonCopyableOrMovableClass {
  public:
    uint32_t getNumPorts() override;

    WddmFabricDeviceImp() = delete;
    WddmFabricDeviceImp(OsSysman *pOsSysman);
    ~WddmFabricDeviceImp() override;

  private:
    uint32_t numPorts = 0;
};

class WddmFabricPortImp : public OsFabricPort, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getProperties(zes_fabric_port_properties_t *pProperties) override;
    ze_result_t getLinkType(zes_fabric_link_type_t *pLinkType) override;
    ze_result_t getConfig(zes_fabric_port_config_t *pConfig) override;
    ze_result_t setConfig(const zes_fabric_port_config_t *pConfig) override;
    ze_result_t getState(zes_fabric_port_state_t *pState) override;
    ze_result_t getThroughput(zes_fabric_port_throughput_t *pThroughput) override;

    WddmFabricPortImp() = delete;
    WddmFabricPortImp(OsFabricDevice *pOsFabricDevice, uint32_t portNum);
    ~WddmFabricPortImp() override;
};

} // namespace L0
