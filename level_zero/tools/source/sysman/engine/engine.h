/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/sysman/zes_handles_struct.h"
#include "level_zero/core/source/device/device.h"
#include <level_zero/zes_api.h>

#include <mutex>
#include <vector>

namespace L0 {
using EngineInstanceSubDeviceId = std::pair<uint32_t, uint32_t>;
struct OsSysman;

class Engine : _zes_engine_handle_t {
  public:
    virtual ~Engine() = default;
    virtual ze_result_t engineGetProperties(zes_engine_properties_t *pProperties) = 0;
    virtual ze_result_t engineGetActivity(zes_engine_stats_t *pStats) = 0;
    virtual ze_result_t engineGetActivityExt(uint32_t *pCount, zes_engine_stats_t *pStats) = 0;

    static Engine *fromHandle(zes_engine_handle_t handle) {
        return static_cast<Engine *>(handle);
    }
    inline zes_engine_handle_t toHandle() { return this; }
    ze_result_t initStatus = ZE_RESULT_SUCCESS;
};

struct EngineHandleContext {
    EngineHandleContext(OsSysman *pOsSysman);
    MOCKABLE_VIRTUAL ~EngineHandleContext();

    MOCKABLE_VIRTUAL void init(std::vector<ze_device_handle_t> &deviceHandles);
    void releaseEngines();

    ze_result_t engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine);

    OsSysman *pOsSysman = nullptr;
    std::vector<std::unique_ptr<Engine>> handleList = {};
    bool isEngineInitDone() {
        return engineInitDone;
    }

  private:
    void createHandle(zes_engine_group_t engineType, uint32_t engineInstance, uint32_t subDeviceId, ze_bool_t onSubdevice);
    std::once_flag initEngineOnce;
    bool engineInitDone = false;
    ze_result_t deviceEngineInitStatus = ZE_RESULT_SUCCESS;
};

} // namespace L0
