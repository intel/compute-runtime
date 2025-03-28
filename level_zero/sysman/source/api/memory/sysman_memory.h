/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/sysman/zes_handles_struct.h"
#include <level_zero/zes_api.h>

#include <memory>
#include <mutex>
#include <vector>

namespace L0 {
namespace Sysman {

struct OsSysman;

class Memory : _zes_mem_handle_t {
  public:
    virtual ~Memory() = default;
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

    ze_result_t init(uint32_t subDeviceCount);

    ze_result_t memoryGet(uint32_t *pCount, zes_mem_handle_t *phMemory);
    void releaseMemoryHandles();

    OsSysman *pOsSysman = nullptr;
    std::vector<std::unique_ptr<Memory>> handleList = {};

    bool isMemoryInitDone() {
        return memoryInitDone;
    }

  private:
    void createHandle(bool onSubdevice, uint32_t subDeviceId);
    std::once_flag initMemoryOnce;
    bool memoryInitDone = false;
};

} // namespace Sysman
} // namespace L0
