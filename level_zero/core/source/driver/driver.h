/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include <vector>

namespace L0 {
struct Driver {
    virtual ze_result_t driverInit() = 0;
    virtual void initialize(ze_result_t *result) = 0;
    static Driver *get() { return driver; }
    virtual ~Driver() = default;
    virtual void tryInitGtpin() = 0;
    virtual ze_result_t driverHandleGet(uint32_t *pCount, ze_driver_handle_t *phDrivers) = 0;
    virtual unsigned int getPid() const = 0;

  protected:
    static Driver *driver;
};

ze_result_t init(ze_init_flags_t);
ze_result_t initDrivers(uint32_t *pCount, ze_driver_handle_t *phDrivers, ze_init_driver_type_desc_t *desc);

extern bool sysmanInitFromCore;
extern uint32_t driverCount;
extern std::vector<_ze_driver_handle_t *> *globalDriverHandles;
extern bool levelZeroDriverInitialized;
} // namespace L0
