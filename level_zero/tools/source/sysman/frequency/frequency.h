/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

#include <vector>

struct _zes_freq_handle_t {
    virtual ~_zes_freq_handle_t() = default;
};

namespace L0 {

struct OsSysman;

class Frequency : _zes_freq_handle_t {
  public:
    virtual ~Frequency() {}

    virtual ze_result_t frequencyGetProperties(zes_freq_properties_t *pProperties) = 0;
    virtual ze_result_t frequencyGetAvailableClocks(uint32_t *pCount, double *phFrequency) = 0;
    virtual ze_result_t frequencyGetRange(zes_freq_range_t *pLimits) = 0;
    virtual ze_result_t frequencySetRange(const zes_freq_range_t *pLimits) = 0;
    virtual ze_result_t frequencyGetState(zes_freq_state_t *pState) = 0;
    virtual ze_result_t frequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) = 0;

    static Frequency *fromHandle(zes_freq_handle_t handle) {
        return static_cast<Frequency *>(handle);
    }
    inline zes_freq_handle_t toZesFreqHandle() { return this; }
};

struct FrequencyHandleContext {
    FrequencyHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~FrequencyHandleContext();

    ze_result_t init();

    ze_result_t frequencyGet(uint32_t *pCount, zes_freq_handle_t *phFrequency);

    OsSysman *pOsSysman;
    std::vector<Frequency *> handleList = {};
};

} // namespace L0
