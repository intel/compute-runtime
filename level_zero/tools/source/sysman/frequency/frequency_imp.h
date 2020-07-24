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
#include <level_zero/zet_api.h>

namespace L0 {

class FrequencyImp : public Frequency, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t frequencyGetProperties(zet_freq_properties_t *pProperties) override;
    ze_result_t frequencyGetAvailableClocks(uint32_t *pCount, double *phFrequency) override;
    ze_result_t frequencyGetRange(zet_freq_range_t *pLimits) override;
    ze_result_t frequencySetRange(const zet_freq_range_t *pLimits) override;
    ze_result_t frequencyGetState(zet_freq_state_t *pState) override;

    ze_result_t frequencyGetProperties(zes_freq_properties_t *pProperties) override;
    ze_result_t frequencyGetRange(zes_freq_range_t *pLimits) override;
    ze_result_t frequencySetRange(const zes_freq_range_t *pLimits) override;
    ze_result_t frequencyGetState(zes_freq_state_t *pState) override;
    ze_result_t frequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) override;

    FrequencyImp() = default;
    FrequencyImp(OsSysman *pOsSysman);
    ~FrequencyImp() override;
    OsFrequency *pOsFrequency = nullptr;
    void init();

  private:
    static const double step;
    static const bool canControl;

    zet_freq_properties_t frequencyProperties = {};
    zes_freq_properties_t zesFrequencyProperties = {};
    double *pClocks = nullptr;
    uint32_t numClocks = 0;
};

} // namespace L0
