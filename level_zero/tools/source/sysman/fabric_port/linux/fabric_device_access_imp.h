/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "fabric_device_access.h"
#include "iaf_nl_api.h"
#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

struct Port {
    bool onSubdevice;
    zes_fabric_port_id_t portId;
    std::string model;
    zes_fabric_port_speed_t maxRxSpeed;
    zes_fabric_port_speed_t maxTxSpeed;
};

class FabricDeviceAccessNl : public FabricDeviceAccess {
  public:
    FabricDeviceAccessNl() = delete;
    FabricDeviceAccessNl(OsSysman *pOsSysman);
    ~FabricDeviceAccessNl() override;

    ze_result_t getState(const zes_fabric_port_id_t portId, zes_fabric_port_state_t &state) override;
    ze_result_t getThroughput(const zes_fabric_port_id_t portId, zes_fabric_port_throughput_t &througput) override;

    ze_result_t getPortEnabledState(const zes_fabric_port_id_t portId, bool &enabled) override;
    ze_result_t getPortBeaconState(const zes_fabric_port_id_t portId, bool &enabled) override;
    ze_result_t enablePortBeaconing(const zes_fabric_port_id_t portId) override;
    ze_result_t disablePortBeaconing(const zes_fabric_port_id_t portId) override;
    ze_result_t enable(const zes_fabric_port_id_t portId) override;
    ze_result_t disable(const zes_fabric_port_id_t portId) override;
    ze_result_t enableUsage(const zes_fabric_port_id_t portId) override;
    ze_result_t disableUsage(const zes_fabric_port_id_t portId) override;
    ze_result_t forceSweep() override;
    ze_result_t routingQuery(uint32_t &start, uint32_t &end) override;

    ze_result_t getPorts(std::vector<zes_fabric_port_id_t> &ports) override;
    void getProperties(const zes_fabric_port_id_t portId, std::string &model, bool &onSubdevice,
                       uint32_t &subdeviceId, zes_fabric_port_speed_t &maxRxSpeed, zes_fabric_port_speed_t &maxTxSpeed) override;

  private:
    ze_result_t init();
    ze_result_t initMyPorts(const uint32_t fabricId);
    void populateGuidMap();
    ze_result_t getAllFabricIds(std::vector<uint32_t> &fabricIds);
    ze_result_t getNumSubdevices(const uint32_t fabricId, uint32_t &numSubdevices);
    ze_result_t getSubdevice(const uint32_t fabricId, const uint32_t subdeviceId, uint64_t &guid, std::vector<uint8_t> &ports);
    ze_result_t getPortSpeeds(const zes_fabric_port_id_t portId, zes_fabric_port_speed_t &maxRxSpeed, zes_fabric_port_speed_t &maxTxSpeed);

    LinuxSysmanImp *pLinuxSysmanImp = nullptr;

    std::map<uint64_t, zes_fabric_port_id_t> guidMap = {};

  protected:
    IafNlApi *pIafNlApi = nullptr;
    std::vector<Port> myPorts = {};
};

} // namespace L0
