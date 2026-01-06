/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/engine/sysman_engine.h"
#include <level_zero/zes_api.h>

#include <set>

namespace L0 {
namespace Sysman {

struct OsSysman;

class OsEngine {
  public:
    virtual ze_result_t getActivity(zes_engine_stats_t *pStats) = 0;
    virtual ze_result_t getActivityExt(uint32_t *pCount, zes_engine_stats_t *pStats) = 0;
    virtual ze_result_t getProperties(zes_engine_properties_t &properties) = 0;
    virtual bool isEngineModuleSupported() = 0;
    static std::unique_ptr<OsEngine> create(OsSysman *pOsSysman, MapOfEngineInfo &mapEngineInfo, zes_engine_group_t engineType, uint32_t engineInstance, uint32_t tileId, ze_bool_t onSubdevice);
    static ze_result_t getNumEngineTypeAndInstances(MapOfEngineInfo &mapEngineInfo, OsSysman *pOsSysman);
    virtual ~OsEngine() = default;
};

} // namespace Sysman

} // namespace L0
