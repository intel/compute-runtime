/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

#include "third_party/level_zero/zes_api_ext.h"

#include <vector>

struct _zet_sysman_engine_handle_t {
    virtual ~_zet_sysman_engine_handle_t() = default;
};

struct _zes_engine_handle_t {
    virtual ~_zes_engine_handle_t() = default;
};

namespace L0 {

struct OsSysman;

class Engine : _zet_sysman_engine_handle_t, _zes_engine_handle_t {
  public:
    virtual ze_result_t engineGetProperties(zet_engine_properties_t *pProperties) = 0;
    virtual ze_result_t engineGetActivity(zet_engine_stats_t *pStats) = 0;

    virtual ze_result_t engineGetProperties(zes_engine_properties_t *pProperties) = 0;
    virtual ze_result_t engineGetActivity(zes_engine_stats_t *pStats) = 0;

    static Engine *fromHandle(zet_sysman_engine_handle_t handle) {
        return static_cast<Engine *>(handle);
    }
    static Engine *fromHandle(zes_engine_handle_t handle) {
        return static_cast<Engine *>(handle);
    }
    inline zet_sysman_engine_handle_t toZetHandle() { return this; }
    inline zes_engine_handle_t toHandle() { return this; }
    bool initSuccess = false;
};

struct EngineHandleContext {
    EngineHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~EngineHandleContext();

    void init();

    ze_result_t engineGet(uint32_t *pCount, zet_sysman_engine_handle_t *phEngine);
    ze_result_t engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine);

    OsSysman *pOsSysman = nullptr;
    std::vector<Engine *> handleList = {};

  private:
    void createHandle(zet_engine_group_t type);
    void createHandle(zes_engine_group_t type);
};

} // namespace L0
