/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"

#include <level_zero/zes_api.h>

#include <vector>

namespace L0 {
namespace Sysman {

struct SysmanDeviceImp;

struct OsSysman {
    virtual ~OsSysman(){};

    virtual ze_result_t init() = 0;
    static OsSysman *create(SysmanDeviceImp *pSysmanImp);
    virtual uint32_t getSubDeviceCount() = 0;
    virtual const NEO::HardwareInfo &getHardwareInfo() const = 0;
    virtual void getDeviceUuids(std::vector<std::string> &deviceUuids) = 0;
};

} // namespace Sysman
} // namespace L0
