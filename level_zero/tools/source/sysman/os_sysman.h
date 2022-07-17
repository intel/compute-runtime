/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

#include <vector>

namespace L0 {

struct SysmanDeviceImp;

struct OsSysman {
    virtual ~OsSysman(){};

    virtual ze_result_t init() = 0;
    static OsSysman *create(SysmanDeviceImp *pSysmanImp);
    virtual std::vector<ze_device_handle_t> &getDeviceHandles() = 0;
    virtual ze_device_handle_t getCoreDeviceHandle() = 0;
};

} // namespace L0
