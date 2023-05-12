/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman.h"

namespace L0 {

class OsFrequency {
  public:
    virtual ze_result_t osFrequencyGetProperties(zes_freq_properties_t &properties) = 0;
    virtual double osFrequencyGetStepSize() = 0;
    virtual ze_result_t osFrequencyGetRange(zes_freq_range_t *pLimits) = 0;
    virtual ze_result_t osFrequencySetRange(const zes_freq_range_t *pLimits) = 0;
    virtual ze_result_t osFrequencyGetState(zes_freq_state_t *pState) = 0;
    virtual ze_result_t osFrequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) = 0;
    virtual ze_result_t getOcCapabilities(zes_oc_capabilities_t *pOcCapabilities) = 0;
    virtual ze_result_t getOcFrequencyTarget(double *pCurrentOcFrequency) = 0;
    virtual ze_result_t setOcFrequencyTarget(double currentOcFrequency) = 0;
    virtual ze_result_t getOcVoltageTarget(double *pCurrentVoltageTarget, double *pCurrentVoltageOffset) = 0;
    virtual ze_result_t setOcVoltageTarget(double currentVoltageTarget, double currentVoltageOffset) = 0;
    virtual ze_result_t getOcMode(zes_oc_mode_t *pCurrentOcMode) = 0;
    virtual ze_result_t setOcMode(zes_oc_mode_t currentOcMode) = 0;
    virtual ze_result_t getOcIccMax(double *pOcIccMax) = 0;
    virtual ze_result_t setOcIccMax(double ocIccMax) = 0;
    virtual ze_result_t getOcTjMax(double *pOcTjMax) = 0;
    virtual ze_result_t setOcTjMax(double ocTjMax) = 0;
    static OsFrequency *create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t type);
    static std::vector<zes_freq_domain_t> getNumberOfFreqDomainsSupported(OsSysman *pOsSysman);
    virtual ~OsFrequency() {}
};

} // namespace L0
