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
#include <level_zero/zes_api.h>

namespace L0 {

class FabricDeviceImp : public FabricDevice, NEO::NonCopyableOrMovableClass {
  public:
    FabricDeviceImp() = delete;
    FabricDeviceImp(OsSysman *pOsSysman);
    ~FabricDeviceImp() override;
    uint32_t getNumPorts() override;
    OsFabricDevice *getOsFabricDevice() override { return pOsFabricDevice; }

  protected:
    OsFabricDevice *pOsFabricDevice = nullptr;
};

class FabricPortImp : public FabricPort, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t fabricPortGetProperties(zes_fabric_port_properties_t *pProperties) override;
    ze_result_t fabricPortGetLinkType(zes_fabric_link_type_t *pLinkType) override;
    ze_result_t fabricPortGetConfig(zes_fabric_port_config_t *pConfig) override;
    ze_result_t fabricPortSetConfig(const zes_fabric_port_config_t *pConfig) override;
    ze_result_t fabricPortGetState(zes_fabric_port_state_t *pState) override;
    ze_result_t fabricPortGetThroughput(zes_fabric_port_throughput_t *pThroughput) override;

    FabricPortImp() = delete;
    FabricPortImp(FabricDevice *pFabricDevice, uint32_t portNum);
    ~FabricPortImp() override;

  protected:
    void init();
    OsFabricPort *pOsFabricPort = nullptr;
};

} // namespace L0
