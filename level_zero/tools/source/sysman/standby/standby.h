/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/sysman/zes_handles_struct.h"
#include "level_zero/core/source/device/device.h"
#include <level_zero/zes_api.h>

#include <mutex>
#include <vector>

namespace L0 {

struct OsSysman;

class Standby : _zes_standby_handle_t {
  public:
    virtual ~Standby() = default;
    virtual ze_result_t standbyGetProperties(zes_standby_properties_t *pProperties) = 0;
    virtual ze_result_t standbyGetMode(zes_standby_promo_mode_t *pMode) = 0;
    virtual ze_result_t standbySetMode(const zes_standby_promo_mode_t mode) = 0;

    inline zes_standby_handle_t toStandbyHandle() { return this; }

    static Standby *fromHandle(zes_standby_handle_t handle) {
        return static_cast<Standby *>(handle);
    }
    bool isStandbyEnabled = false;
};

struct StandbyHandleContext {
    StandbyHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~StandbyHandleContext();

    ze_result_t init(std::vector<ze_device_handle_t> &deviceHandles);

    ze_result_t standbyGet(uint32_t *pCount, zes_standby_handle_t *phStandby);

    OsSysman *pOsSysman;
    std::vector<std::unique_ptr<Standby>> handleList = {};

  private:
    void createHandle(ze_device_handle_t deviceHandle);
    std::once_flag initStandbyOnce;
};

} // namespace L0
