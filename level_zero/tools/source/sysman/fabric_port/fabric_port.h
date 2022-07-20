/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <level_zero/zes_api.h>

#include <mutex>
#include <vector>

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

class FabricPort : _zes_fabric_port_handle_t {
  public:
    ~FabricPort() override = default;
    virtual ze_result_t fabricPortGetProperties(zes_fabric_port_properties_t *pProperties) = 0;
    virtual ze_result_t fabricPortGetLinkType(zes_fabric_link_type_t *pLinkType) = 0;
    virtual ze_result_t fabricPortGetConfig(zes_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t fabricPortSetConfig(const zes_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t fabricPortGetState(zes_fabric_port_state_t *pState) = 0;
    virtual ze_result_t fabricPortGetThroughput(zes_fabric_port_throughput_t *pThroughput) = 0;

    inline zes_fabric_port_handle_t toZesHandle() { return this; }

    static FabricPort *fromHandle(zes_fabric_port_handle_t handle) {
        return static_cast<FabricPort *>(handle);
    }
};

struct FabricPortHandleContext : NEO::NonCopyableOrMovableClass {
    FabricPortHandleContext(OsSysman *pOsSysman);
    ~FabricPortHandleContext();

    ze_result_t init();

    ze_result_t fabricPortGet(uint32_t *pCount, zes_fabric_port_handle_t *phPort);

    FabricDevice *pFabricDevice = nullptr;
    std::vector<FabricPort *> handleList = {};

  private:
    std::once_flag initFabricPortOnce;
};

} // namespace L0
