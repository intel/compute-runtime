/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "fabric_device_access.h"
#include "sysman/fabric_port/fabric_port_imp.h"
#include "sysman/fabric_port/os_fabric_port.h"

#include <vector>

namespace L0 {

class LinuxFabricDeviceImp : public OsFabricDevice, NEO::NonCopyableOrMovableClass {
  public:
    uint32_t getNumPorts() override;

    ze_result_t performSweep();
    ze_result_t getPortEnabledState(const zes_fabric_port_id_t portId, bool &enabled);
    ze_result_t enablePort(const zes_fabric_port_id_t portId);
    ze_result_t disablePort(const zes_fabric_port_id_t portId);
    ze_result_t getPortBeaconState(const zes_fabric_port_id_t portId, bool &enabled);
    ze_result_t enablePortBeaconing(const zes_fabric_port_id_t portId);
    ze_result_t disablePortBeaconing(const zes_fabric_port_id_t portId);
    ze_result_t getState(const zes_fabric_port_id_t portId, zes_fabric_port_state_t *pState);
    ze_result_t getThroughput(const zes_fabric_port_id_t portId, zes_fabric_port_throughput_t *pThroughput);

    void getPortId(const uint32_t portNumber, zes_fabric_port_id_t &portId);
    void getProperties(const zes_fabric_port_id_t portId, std::string &model, bool &onSubdevice,
                       uint32_t &subdeviceId, zes_fabric_port_speed_t &maxRxSpeed, zes_fabric_port_speed_t &maxTxSpeed);

    LinuxFabricDeviceImp() = delete;
    LinuxFabricDeviceImp(OsSysman *pOsSysman);
    ~LinuxFabricDeviceImp() override;

  private:
    std::vector<zes_fabric_port_id_t> portIds = {};

    ze_result_t forceSweep();
    ze_result_t routingQuery(uint32_t &start, uint32_t &end);
    ze_result_t enable(const zes_fabric_port_id_t portId);
    ze_result_t disable(const zes_fabric_port_id_t portId);
    ze_result_t enableUsage(const zes_fabric_port_id_t portId);
    ze_result_t disableUsage(const zes_fabric_port_id_t portId);

  protected:
    FabricDeviceAccess *pFabricDeviceAccess = nullptr;
};

class LinuxFabricPortImp : public OsFabricPort, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getProperties(zes_fabric_port_properties_t *pProperties) override;
    ze_result_t getLinkType(zes_fabric_link_type_t *pLinkType) override;
    ze_result_t getConfig(zes_fabric_port_config_t *pConfig) override;
    ze_result_t setConfig(const zes_fabric_port_config_t *pConfig) override;
    ze_result_t getState(zes_fabric_port_state_t *pState) override;
    ze_result_t getThroughput(zes_fabric_port_throughput_t *pThroughput) override;

    LinuxFabricPortImp() = delete;
    LinuxFabricPortImp(OsFabricDevice *pOsFabricDevice, uint32_t portNum);
    ~LinuxFabricPortImp() override;

  private:
    LinuxFabricDeviceImp *pLinuxFabricDeviceImp = nullptr;

    uint32_t portNum = 0;
    zes_fabric_port_id_t portId = {};
    std::string model = "";
    bool onSubdevice = false;
    uint32_t subdeviceId;
    zes_fabric_port_speed_t maxRxSpeed = {};
    zes_fabric_port_speed_t maxTxSpeed = {};
};

} // namespace L0
