/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zet_api.h>

namespace L0 {

struct OsSysman;
class OsMemory {
  public:
    virtual ze_result_t getMemorySize(uint64_t &maxSize, uint64_t &allocSize) = 0;
    virtual ze_result_t getMemHealth(zet_mem_health_t &memHealth) = 0;
    static OsMemory *create(OsSysman *pOsSysman);
    virtual ~OsMemory() {}
};

} // namespace L0
