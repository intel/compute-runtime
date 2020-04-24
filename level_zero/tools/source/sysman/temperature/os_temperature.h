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
class OsTemperature {
  public:
    static OsTemperature *create(OsSysman *pOsSysman);
    virtual ~OsTemperature() = default;
};

} // namespace L0
