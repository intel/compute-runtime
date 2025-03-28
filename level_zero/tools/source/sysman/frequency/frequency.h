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

constexpr double unsupportedProperty = -1.0;

struct OsSysman;

class Frequency : _zes_freq_handle_t {
  public:
    virtual ~Frequency() = default;

    virtual ze_result_t frequencyGetProperties(zes_freq_properties_t *pProperties) = 0;
    virtual ze_result_t frequencyGetAvailableClocks(uint32_t *pCount, double *phFrequency) = 0;
    virtual ze_result_t frequencyGetRange(zes_freq_range_t *pLimits) = 0;
    virtual ze_result_t frequencySetRange(const zes_freq_range_t *pLimits) = 0;
    virtual ze_result_t frequencyGetState(zes_freq_state_t *pState) = 0;
    virtual ze_result_t frequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) = 0;

    // Overclocking
    virtual ze_result_t frequencyOcGetCapabilities(zes_oc_capabilities_t *pOcCapabilities) = 0;
    virtual ze_result_t frequencyOcGetFrequencyTarget(double *pCurrentOcfrequency) = 0;
    virtual ze_result_t frequencyOcSetFrequencyTarget(double currentOcfrequency) = 0;
    virtual ze_result_t frequencyOcGetVoltageTarget(double *pCurrentVoltageTarget, double *pCurrentVoltageOffset) = 0;
    virtual ze_result_t frequencyOcSetVoltageTarget(double currentVoltageTarget, double currentVoltageOffset) = 0;
    virtual ze_result_t frequencyOcGetMode(zes_oc_mode_t *pCurrentOcMode) = 0;
    virtual ze_result_t frequencyOcSetMode(zes_oc_mode_t currentOcMode) = 0;
    virtual ze_result_t frequencyOcGetIccMax(double *pOcIccMax) = 0;
    virtual ze_result_t frequencyOcSetIccMax(double ocIccMax) = 0;
    virtual ze_result_t frequencyOcGeTjMax(double *pOcTjMax) = 0;
    virtual ze_result_t frequencyOcSetTjMax(double ocTjMax) = 0;

    static Frequency *fromHandle(zes_freq_handle_t handle) {
        return static_cast<Frequency *>(handle);
    }
    inline zes_freq_handle_t toZesFreqHandle() { return this; }
};

struct FrequencyHandleContext {
    FrequencyHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~FrequencyHandleContext();

    ze_result_t init(std::vector<ze_device_handle_t> &deviceHandles);

    ze_result_t frequencyGet(uint32_t *pCount, zes_freq_handle_t *phFrequency);

    OsSysman *pOsSysman = nullptr;
    std::vector<Frequency *> handleList = {};

  private:
    void createHandle(ze_device_handle_t deviceHandle, zes_freq_domain_t frequencyDomain);
    std::once_flag initFrequencyOnce;
};

} // namespace L0
