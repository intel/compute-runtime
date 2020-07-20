/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <level_zero/zet_api.h>

#include "third_party/level_zero/zes_api_ext.h"

#include <vector>

struct _zet_sysman_fabric_port_handle_t {
    virtual ~_zet_sysman_fabric_port_handle_t() = default;
};
struct _zes_fabric_port_handle_t {
    virtual ~_zes_fabric_port_handle_t() = default;
};

namespace L0 {

struct OsSysman;
class OsFabricDevice;

class FabricDevice {
  public:
    virtual ~FabricDevice() = default;
    virtual OsFabricDevice *getOsFabricDevice() = 0;
    virtual uint32_t getNumPorts() = 0;
};

class FabricPort : _zet_sysman_fabric_port_handle_t, _zes_fabric_port_handle_t {
  public:
    virtual ~FabricPort() = default;
    virtual ze_result_t fabricPortGetProperties(zes_fabric_port_properties_t *pProperties) = 0;
    virtual ze_result_t fabricPortGetLinkType(zes_fabric_link_type_t *pLinkType) = 0;
    virtual ze_result_t fabricPortGetConfig(zes_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t fabricPortSetConfig(const zes_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t fabricPortGetState(zes_fabric_port_state_t *pState) = 0;
    virtual ze_result_t fabricPortGetThroughput(zes_fabric_port_throughput_t *pThroughput) = 0;

    virtual ze_result_t fabricPortGetProperties(zet_fabric_port_properties_t *pProperties) = 0;
    virtual ze_result_t fabricPortGetLinkType(ze_bool_t verbose, zet_fabric_link_type_t *pLinkType) = 0;
    virtual ze_result_t fabricPortGetConfig(zet_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t fabricPortSetConfig(const zet_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t fabricPortGetState(zet_fabric_port_state_t *pState) = 0;
    virtual ze_result_t fabricPortGetThroughput(zet_fabric_port_throughput_t *pThroughput) = 0;

    inline zet_sysman_fabric_port_handle_t toHandle() { return this; }
    inline zes_fabric_port_handle_t toZesHandle() { return this; }

    static FabricPort *fromHandle(zet_sysman_fabric_port_handle_t handle) {
        return static_cast<FabricPort *>(handle);
    }

    static FabricPort *fromHandle(zes_fabric_port_handle_t handle) {
        return static_cast<FabricPort *>(handle);
    }
};

struct FabricPortHandleContext : NEO::NonCopyableOrMovableClass {
    FabricPortHandleContext(OsSysman *pOsSysman);
    ~FabricPortHandleContext();

    ze_result_t init();

    ze_result_t fabricPortGet(uint32_t *pCount, zes_fabric_port_handle_t *phPort);
    ze_result_t fabricPortGet(uint32_t *pCount, zet_sysman_fabric_port_handle_t *phPort);

    FabricDevice *pFabricDevice = nullptr;
    std::vector<FabricPort *> handleList = {};
};

} // namespace L0
