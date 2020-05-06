/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/frequency/frequency.h"
#include "level_zero/tools/source/sysman/frequency/os_frequency.h"
#include <level_zero/zet_api.h>

namespace L0 {

class FrequencyImp : public Frequency {
  public:
    ze_result_t frequencyGetProperties(zet_freq_properties_t *pProperties) override;
    ze_result_t frequencyGetAvailableClocks(uint32_t *pCount, double *phFrequency) override;
    ze_result_t frequencyGetRange(zet_freq_range_t *pLimits) override;
    ze_result_t frequencySetRange(const zet_freq_range_t *pLimits) override;
    ze_result_t frequencyGetState(zet_freq_state_t *pState) override;

    FrequencyImp() = default;
    FrequencyImp(OsSysman *pOsSysman);
    ~FrequencyImp() override;
    OsFrequency *pOsFrequency = nullptr;
    void init();

    // Don't allow copies of the FrequencyImp object
    FrequencyImp(const FrequencyImp &obj) = delete;
    FrequencyImp &operator=(const FrequencyImp &obj) = delete;

  private:
    static const double step;
    static const bool canControl;

    zet_freq_properties_t frequencyProperties = {};
    double *pClocks = nullptr;
    uint32_t numClocks = 0;
};

} // namespace L0
