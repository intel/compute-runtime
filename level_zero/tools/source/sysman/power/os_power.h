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
class OsPower {
  public:
    static OsPower *create(OsSysman *pOsSysman);
    virtual ~OsPower() = default;
};

} // namespace L0
