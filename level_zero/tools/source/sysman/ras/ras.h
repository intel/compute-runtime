/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

#include <vector>

struct _zet_sysman_ras_handle_t {
    virtual ~_zet_sysman_ras_handle_t() = default;
};

namespace L0 {

struct OsSysman;

class Ras : _zet_sysman_ras_handle_t {
  public:
    virtual ze_result_t rasGetProperties(zet_ras_properties_t *pProperties) = 0;
    virtual ze_result_t rasGetConfig(zet_ras_config_t *pConfig) = 0;
    virtual ze_result_t rasSetConfig(const zet_ras_config_t *pConfig) = 0;
    virtual ze_result_t rasGetState(ze_bool_t clear, uint64_t *pTotalErrors, zet_ras_details_t *pDetails) = 0;

    static Ras *fromHandle(zet_sysman_ras_handle_t handle) {
        return static_cast<Ras *>(handle);
    }
    inline zet_sysman_ras_handle_t toHandle() { return this; }
};

struct RasHandleContext {
    RasHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~RasHandleContext();

    void init();

    ze_result_t rasGet(uint32_t *pCount, zet_sysman_ras_handle_t *phRas);

    OsSysman *pOsSysman = nullptr;
    std::vector<Ras *> handleList;
};

} // namespace L0
