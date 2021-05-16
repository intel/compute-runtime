/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>
namespace L0 {

struct SysmanDeviceImp;

struct OsSysman {
    virtual ~OsSysman(){};

    virtual ze_result_t init() = 0;
    static OsSysman *create(SysmanDeviceImp *pSysmanImp);
};

} // namespace L0
