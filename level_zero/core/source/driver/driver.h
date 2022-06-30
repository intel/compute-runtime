/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include <memory>

namespace L0 {
struct Driver {
    virtual ze_result_t driverInit(ze_init_flags_t flags) = 0;
    virtual void initialize(ze_result_t *result) = 0;
    static Driver *get() { return driver; }
    virtual ~Driver() = default;

  protected:
    static Driver *driver;
};

ze_result_t init(ze_init_flags_t);
ze_result_t driverHandleGet(uint32_t *pCount, ze_driver_handle_t *phDrivers);

extern uint32_t driverCount;
extern _ze_driver_handle_t *GlobalDriverHandle;
} // namespace L0
