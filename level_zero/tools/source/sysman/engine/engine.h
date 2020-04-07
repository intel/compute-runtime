/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

#include <vector>

struct _zet_sysman_engine_handle_t {
    virtual ~_zet_sysman_engine_handle_t() = default;
};

namespace L0 {

struct OsSysman;

class Engine : _zet_sysman_engine_handle_t {
  public:
    virtual ze_result_t engineGetProperties(zet_engine_properties_t *pProperties) = 0;
    virtual ze_result_t engineGetActivity(zet_engine_stats_t *pStats) = 0;

    static Engine *fromHandle(zet_sysman_engine_handle_t handle) {
        return static_cast<Engine *>(handle);
    }
    inline zet_sysman_engine_handle_t toHandle() { return this; }
};

struct EngineHandleContext {
    EngineHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~EngineHandleContext();

    ze_result_t init();

    ze_result_t engineGet(uint32_t *pCount, zet_sysman_engine_handle_t *phEngine);

    OsSysman *pOsSysman;
    std::vector<Engine *> handleList;
    ze_device_handle_t hCoreDevice;
};

} // namespace L0
