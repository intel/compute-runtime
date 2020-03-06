/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

#include <vector>

struct _zet_sysman_freq_handle_t {};

namespace L0 {

struct OsSysman;

class Frequency : _zet_sysman_freq_handle_t {
  public:
    virtual ~Frequency() {}
    virtual ze_result_t frequencyGetProperties(zet_freq_properties_t *pProperties) = 0;
    virtual ze_result_t frequencyGetRange(zet_freq_range_t *pLimits) = 0;
    virtual ze_result_t frequencySetRange(const zet_freq_range_t *pLimits) = 0;
    virtual ze_result_t frequencyGetState(zet_freq_state_t *pState) = 0;

    static Frequency *fromHandle(zet_sysman_freq_handle_t handle) {
        return static_cast<Frequency *>(handle);
    }
    inline zet_sysman_freq_handle_t toHandle() { return this; }
};

struct FrequencyHandleContext {
    FrequencyHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~FrequencyHandleContext();

    ze_result_t init();

    ze_result_t frequencyGet(uint32_t *pCount, zet_sysman_freq_handle_t *phFrequency);

    OsSysman *pOsSysman;
    std::vector<Frequency *> handle_list;
};

} // namespace L0
