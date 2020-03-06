/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

#include <vector>

struct _zet_sysman_standby_handle_t {};

namespace L0 {

struct OsSysman;

class Standby : _zet_sysman_standby_handle_t {
  public:
    virtual ~Standby() {}
    virtual ze_result_t standbyGetProperties(zet_standby_properties_t *pProperties) = 0;
    virtual ze_result_t standbyGetMode(zet_standby_promo_mode_t *pMode) = 0;
    virtual ze_result_t standbySetMode(const zet_standby_promo_mode_t mode) = 0;

    static Standby *fromHandle(zet_sysman_standby_handle_t handle) {
        return static_cast<Standby *>(handle);
    }
    inline zet_sysman_standby_handle_t toHandle() { return this; }
};

struct StandbyHandleContext {
    StandbyHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~StandbyHandleContext();

    ze_result_t init();

    ze_result_t standbyGet(uint32_t *pCount, zet_sysman_standby_handle_t *phStandby);

    OsSysman *pOsSysman;
    std::vector<Standby *> handle_list;
};

} // namespace L0
