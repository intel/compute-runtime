/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/frequency/frequency.h"
#include "level_zero/tools/source/sysman/frequency/os_frequency.h"
#include <level_zero/zes_api.h>

namespace L0 {

class FrequencyImp : public Frequency, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t frequencyGetProperties(zes_freq_properties_t *pProperties) override;
    ze_result_t frequencyGetAvailableClocks(uint32_t *pCount, double *phFrequency) override;
    ze_result_t frequencyGetRange(zes_freq_range_t *pLimits) override;
    ze_result_t frequencySetRange(const zes_freq_range_t *pLimits) override;
    ze_result_t frequencyGetState(zes_freq_state_t *pState) override;
    ze_result_t frequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) override;

    FrequencyImp() = default;
    FrequencyImp(OsSysman *pOsSysman, ze_device_handle_t handle, uint16_t frequencyDomain);
    ~FrequencyImp() override;
    OsFrequency *pOsFrequency = nullptr;
    void init();

  private:
    static const double step;

    zes_freq_properties_t zesFrequencyProperties = {};
    double *pClocks = nullptr;
    uint32_t numClocks = 0;
    ze_device_handle_t deviceHandle = nullptr;
    uint16_t frequencyDomain = 0;
};

} // namespace L0
