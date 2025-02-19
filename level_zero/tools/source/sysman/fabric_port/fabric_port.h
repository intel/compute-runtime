/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/api/sysman/zes_handles_struct.h"
#include <level_zero/zes_api.h>

#include <memory>
#include <mutex>
#include <vector>

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
    virtual ~FabricPort() = default;
    virtual ze_result_t fabricPortGetProperties(zes_fabric_port_properties_t *pProperties) = 0;
    virtual ze_result_t fabricPortGetLinkType(zes_fabric_link_type_t *pLinkType) = 0;
    virtual ze_result_t fabricPortGetConfig(zes_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t fabricPortSetConfig(const zes_fabric_port_config_t *pConfig) = 0;
    virtual ze_result_t fabricPortGetState(zes_fabric_port_state_t *pState) = 0;
    virtual ze_result_t fabricPortGetThroughput(zes_fabric_port_throughput_t *pThroughput) = 0;
    virtual ze_result_t fabricPortGetErrorCounters(zes_fabric_port_error_counters_t *pErrors) = 0;

    inline zes_fabric_port_handle_t toZesHandle() { return this; }

    static FabricPort *fromHandle(zes_fabric_port_handle_t handle) {
        return static_cast<FabricPort *>(handle);
    }
};

struct FabricPortHandleContext : NEO::NonCopyableAndNonMovableClass {
    FabricPortHandleContext(OsSysman *pOsSysman);
    ~FabricPortHandleContext();

    ze_result_t init();

    ze_result_t fabricPortGet(uint32_t *pCount, zes_fabric_port_handle_t *phPort);
    ze_result_t fabricPortGetMultiPortThroughput(uint32_t numPorts, zes_fabric_port_handle_t *phPort, zes_fabric_port_throughput_t **pThroughput);

    FabricDevice *pFabricDevice = nullptr;
    std::vector<std::unique_ptr<FabricPort>> handleList = {};

  private:
    std::once_flag initFabricPortOnce;
};

} // namespace L0
