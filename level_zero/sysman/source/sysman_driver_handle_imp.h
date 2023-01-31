/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/sysman/source/sysman_driver_handle.h"

namespace L0 {
namespace Sysman {
struct SysmanDevice;
struct SysmanDriverHandleImp : SysmanDriverHandle {
    ~SysmanDriverHandleImp() override;
    SysmanDriverHandleImp();
    ze_result_t initialize(NEO::ExecutionEnvironment &executionEnvironment);
    ze_result_t getDevice(uint32_t *pCount, zes_device_handle_t *phDevices) override;
    std::vector<SysmanDevice *> sysmanDevices;
    uint32_t numDevices = 0;
};

extern struct SysmanDriverHandleImp *GlobalDriver;

} // namespace Sysman
} // namespace L0
