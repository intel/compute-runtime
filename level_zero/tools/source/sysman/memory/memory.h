/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

#include <vector>

struct _zet_sysman_mem_handle_t {
    virtual ~_zet_sysman_mem_handle_t() = default;
};

namespace L0 {

struct OsSysman;

class Memory : _zet_sysman_mem_handle_t {
  public:
    virtual ze_result_t memoryGetProperties(zet_mem_properties_t *pProperties) = 0;
    virtual ze_result_t memoryGetBandwidth(zet_mem_bandwidth_t *pBandwidth) = 0;
    virtual ze_result_t memoryGetState(zet_mem_state_t *pState) = 0;

    static Memory *fromHandle(zet_sysman_mem_handle_t handle) {
        return static_cast<Memory *>(handle);
    }
    inline zet_sysman_mem_handle_t toHandle() { return this; }
};

struct MemoryHandleContext {
    MemoryHandleContext(OsSysman *pOsSysman, ze_device_handle_t hCoreDevice) : pOsSysman(pOsSysman), hCoreDevice(hCoreDevice){};
    ~MemoryHandleContext();

    ze_result_t init();

    ze_result_t memoryGet(uint32_t *pCount, zet_sysman_mem_handle_t *phMemory);

    OsSysman *pOsSysman;
    std::vector<Memory *> handleList;
    ze_device_handle_t hCoreDevice;
};

} // namespace L0
