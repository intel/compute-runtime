/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

#include <vector>

struct _zes_standby_handle_t {
    virtual ~_zes_standby_handle_t() = default;
};

namespace L0 {

struct OsSysman;

class Standby : _zes_standby_handle_t {
  public:
    virtual ~Standby() {}
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

    void init();

    ze_result_t standbyGet(uint32_t *pCount, zes_standby_handle_t *phStandby);

    OsSysman *pOsSysman;
    std::vector<Standby *> handleList = {};
};

} // namespace L0
