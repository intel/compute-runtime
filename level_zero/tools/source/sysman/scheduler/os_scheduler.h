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
class OsScheduler {
  public:
    static OsScheduler *create(OsSysman *pOsSysman);
    virtual ~OsScheduler() {}
};

} // namespace L0
