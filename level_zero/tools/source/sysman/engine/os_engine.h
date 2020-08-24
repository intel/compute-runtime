/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

#include <map>

namespace L0 {

struct OsSysman;
using namespace std;

class OsEngine {
  public:
    virtual ze_result_t getActivity(zes_engine_stats_t *pStats) = 0;
    virtual ze_result_t getProperties(zes_engine_properties_t &properties) = 0;
    static OsEngine *create(OsSysman *pOsSysman, zes_engine_group_t engineType, uint32_t engineInstance);
    static ze_result_t getNumEngineTypeAndInstances(std::multimap<zes_engine_group_t, uint32_t> &engineGroupInstance, OsSysman *pOsSysman);
    virtual ~OsEngine() = default;
};

} // namespace L0
