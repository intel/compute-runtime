/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/device/device.h"
#include <level_zero/zes_api.h>

#include <vector>

struct _zes_mem_handle_t {
    virtual ~_zes_mem_handle_t() = default;
};

namespace L0 {

struct OsSysman;

class Memory : _zes_mem_handle_t {
  public:
    virtual ze_result_t memoryGetProperties(zes_mem_properties_t *pProperties) = 0;
    virtual ze_result_t memoryGetBandwidth(zes_mem_bandwidth_t *pBandwidth) = 0;
    virtual ze_result_t memoryGetState(zes_mem_state_t *pState) = 0;

    static Memory *fromHandle(zes_mem_handle_t handle) {
        return static_cast<Memory *>(handle);
    }
    inline zes_mem_handle_t toHandle() { return this; }
    bool initSuccess = false;
};

struct MemoryHandleContext {
    MemoryHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~MemoryHandleContext();

    ze_result_t init(std::vector<ze_device_handle_t> &deviceHandles);

    ze_result_t memoryGet(uint32_t *pCount, zes_mem_handle_t *phMemory);

    OsSysman *pOsSysman = nullptr;
    bool isLmemSupported = false;
    std::vector<Memory *> handleList = {};

  private:
    void createHandle(ze_device_handle_t deviceHandle);
};

} // namespace L0
