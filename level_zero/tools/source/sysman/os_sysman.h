/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

namespace L0 {

struct SysmanImp;

struct OsSysman {
    virtual ~OsSysman(){};

    virtual ze_result_t init() = 0;
    static OsSysman *create(SysmanImp *pSysmanImp);
};

} // namespace L0
