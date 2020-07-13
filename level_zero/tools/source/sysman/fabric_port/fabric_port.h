/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

#include <vector>

struct _zet_sysman_fabric_port_handle_t {};

namespace L0 {

struct OsSysman;

class FabricPort : _zet_sysman_fabric_port_handle_t {
  public:
    virtual ~FabricPort() = default;
    virtual ze_result_t fabricPortGetProperties(zet_fabric_port_properties_t *pProperties) = 0;
    virtual ze_result_t fabricPortGetLinkType(ze_bool_t verbose, zet_fabric_link_type_t *pLinkType) = 0;
    virtual ze_result_t fabricPortGetConfig(zet_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t fabricPortSetConfig(const zet_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t fabricPortGetState(zet_fabric_port_state_t *pState) = 0;
    virtual ze_result_t fabricPortGetThroughput(zet_fabric_port_throughput_t *pThroughput) = 0;

    static FabricPort *fromHandle(zet_sysman_fabric_port_handle_t handle) {
        return static_cast<FabricPort *>(handle);
    }
    inline zet_sysman_fabric_port_handle_t toHandle() { return this; }
};

struct FabricPortHandleContext {
    FabricPortHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~FabricPortHandleContext();

    ze_result_t init();

    ze_result_t fabricPortGet(uint32_t *pCount, zet_sysman_fabric_port_handle_t *phPort);

    OsSysman *pOsSysman = nullptr;
    std::vector<FabricPort *> handleList = {};
};

} // namespace L0
