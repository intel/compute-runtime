/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include <atomic>
#include <mutex>
#include <vector>

namespace L0 {

class Driver {
  public:
    MOCKABLE_VIRTUAL ze_result_t driverInit();
    MOCKABLE_VIRTUAL void initialize(ze_result_t *result);
    static Driver *get() { return driver; }
    virtual ~Driver() = default;
    void tryInitGtpin();
    MOCKABLE_VIRTUAL ze_result_t driverHandleGet(uint32_t *pCount, ze_driver_handle_t *phDrivers);
    unsigned int getPid() const {
        return pid;
    }

  protected:
    static Driver *driver;
    uint32_t pid = 0;
    std::once_flag initDriverOnce;
    static ze_result_t initStatus;
    std::atomic<bool> gtPinInitializationNeeded{false};
    std::mutex gtpinInitMtx;
};

struct L0EnvVariables {
    uint32_t programDebugging;
    bool metrics;
    bool pin;
    bool sysman;
    bool pciIdDeviceOrder;
    bool fp64Emulation;
};

ze_result_t init(ze_init_flags_t);
ze_result_t initDrivers(uint32_t *pCount, ze_driver_handle_t *phDrivers, ze_init_driver_type_desc_t *desc);

extern bool sysmanInitFromCore;
extern uint32_t driverCount;
extern std::vector<_ze_driver_handle_t *> *globalDriverHandles;
extern bool levelZeroDriverInitialized;
} // namespace L0
