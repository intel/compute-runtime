/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {
struct Driver {
    virtual ze_result_t driverInit(_ze_init_flag_t) = 0;
    virtual void initialize(bool *result) = 0;
    static Driver *get() { return driver; }

  protected:
    static Driver *driver;
};

ze_result_t init(_ze_init_flag_t);
ze_result_t driverHandleGet(uint32_t *pCount, ze_driver_handle_t *phDrivers);

extern uint32_t driverCount;
extern ze_driver_handle_t GlobalDrivers[1];
} // namespace L0
