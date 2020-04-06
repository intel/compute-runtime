/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace L0 {

struct OsSysman;
class OsRas {
  public:
    static OsRas *create(OsSysman *pOsSysman);
    virtual ~OsRas() = default;
};

} // namespace L0
