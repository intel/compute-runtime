/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

#include <memory>
namespace L0 {
namespace Sysman {
struct SysmanDriver {
    virtual ze_result_t driverInit() = 0;
    virtual void initialize(ze_result_t *result) = 0;
    static SysmanDriver *get() { return driver; }
    virtual ~SysmanDriver() = default;

  protected:
    static SysmanDriver *driver;
};

ze_result_t init(zes_init_flags_t);
ze_result_t driverHandleGet(uint32_t *pCount, ze_driver_handle_t *phDrivers);

extern uint32_t driverCount;
extern _ze_driver_handle_t *globalSysmanDriverHandle;
extern bool sysmanOnlyInit;

} // namespace Sysman
} // namespace L0
