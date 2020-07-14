/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

#include "third_party/level_zero/zes_api_ext.h"

#include <vector>

struct _zet_sysman_standby_handle_t {
    virtual ~_zet_sysman_standby_handle_t() = default;
};
struct _zes_standby_handle_t {
    virtual ~_zes_standby_handle_t() = default;
};

namespace L0 {

struct OsSysman;

class Standby : _zet_sysman_standby_handle_t, _zes_standby_handle_t {
  public:
    virtual ~Standby() {}
    virtual ze_result_t standbyGetProperties(zet_standby_properties_t *pProperties) = 0;
    virtual ze_result_t standbyGetMode(zet_standby_promo_mode_t *pMode) = 0;
    virtual ze_result_t standbySetMode(const zet_standby_promo_mode_t mode) = 0;

    virtual ze_result_t standbyGetProperties(zes_standby_properties_t *pProperties) = 0;
    virtual ze_result_t standbyGetMode(zes_standby_promo_mode_t *pMode) = 0;
    virtual ze_result_t standbySetMode(const zes_standby_promo_mode_t mode) = 0;

    inline zet_sysman_standby_handle_t toHandle() { return this; }
    inline zes_standby_handle_t toStandbyHandle() { return this; }

    static Standby *fromHandle(zet_sysman_standby_handle_t handle) {
        return static_cast<Standby *>(handle);
    }

    static Standby *fromHandle(zes_standby_handle_t handle) {
        return static_cast<Standby *>(handle);
    }
    bool isStandbyEnabled = false;
};

struct StandbyHandleContext {
    StandbyHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~StandbyHandleContext();

    void init();

    ze_result_t standbyGet(uint32_t *pCount, zet_sysman_standby_handle_t *phStandby);
    ze_result_t standbyGet(uint32_t *pCount, zes_standby_handle_t *phStandby);

    OsSysman *pOsSysman;
    std::vector<Standby *> handleList = {};
};

} // namespace L0
