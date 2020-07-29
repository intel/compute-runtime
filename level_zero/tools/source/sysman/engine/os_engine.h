/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

namespace L0 {

struct OsSysman;
class OsEngine {
  public:
    virtual ze_result_t getActiveTime(uint64_t &activeTime) = 0;
    virtual ze_result_t getTimeStamp(uint64_t &timeStamp) = 0;
    virtual ze_result_t getEngineGroup(zes_engine_group_t &engineGroup) = 0;
    static OsEngine *create(OsSysman *pOsSysman);
    virtual ~OsEngine() = default;
};

} // namespace L0
