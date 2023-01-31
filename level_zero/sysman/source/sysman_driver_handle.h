/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"

#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

#include <vector>

namespace L0 {
namespace Sysman {

struct SysmanDriverHandle : _ze_driver_handle_t {
    static SysmanDriverHandle *fromHandle(zes_driver_handle_t handle) { return static_cast<SysmanDriverHandle *>(handle); }
    inline zes_driver_handle_t toHandle() { return this; }

    SysmanDriverHandle &operator=(const SysmanDriverHandle &) = delete;
    SysmanDriverHandle &operator=(SysmanDriverHandle &&) = delete;

    static SysmanDriverHandle *create(NEO::ExecutionEnvironment &executionEnvironment, ze_result_t *returnValue);
    virtual ze_result_t getDevice(uint32_t *pCount, zes_device_handle_t *phDevices) = 0;
};

} // namespace Sysman
} // namespace L0
