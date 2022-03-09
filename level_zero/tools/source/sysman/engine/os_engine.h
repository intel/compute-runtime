/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/engine/engine.h"
#include <level_zero/zes_api.h>

#include <set>

namespace L0 {

struct OsSysman;
using namespace std;

class OsEngine {
  public:
    virtual ze_result_t getActivity(zes_engine_stats_t *pStats) = 0;
    virtual ze_result_t getProperties(zes_engine_properties_t &properties) = 0;
    virtual bool isEngineModuleSupported() = 0;
    static OsEngine *create(OsSysman *pOsSysman, zes_engine_group_t engineType, uint32_t engineInstance, uint32_t subDeviceId);
    static ze_result_t getNumEngineTypeAndInstances(std::set<std::pair<zes_engine_group_t, EngineInstanceSubDeviceId>> &engineGroupInstance, OsSysman *pOsSysman);
    virtual ~OsEngine() = default;
};

} // namespace L0
