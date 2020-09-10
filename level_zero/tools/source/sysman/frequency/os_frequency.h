/*
 * Copyright (C) 2019-2020 Intel Corporation
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
    virtual ze_result_t osFrequencyGetRange(zes_freq_range_t *pLimits) = 0;
    virtual ze_result_t osFrequencySetRange(const zes_freq_range_t *pLimits) = 0;
    virtual ze_result_t osFrequencyGetState(zes_freq_state_t *pState) = 0;
    virtual ze_result_t osFrequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) = 0;

    static OsFrequency *create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId);
    static uint16_t getHardwareBlockCount(ze_device_handle_t handle);
    virtual ~OsFrequency() {}
};

} // namespace L0
