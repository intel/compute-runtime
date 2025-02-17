/*
 * Copyright (C) 2020-2025 Intel Corporation
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

class FrequencyImp : public Frequency, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t frequencyGetProperties(zes_freq_properties_t *pProperties) override;
    ze_result_t frequencyGetAvailableClocks(uint32_t *pCount, double *phFrequency) override;
    ze_result_t frequencyGetRange(zes_freq_range_t *pLimits) override;
    ze_result_t frequencySetRange(const zes_freq_range_t *pLimits) override;
    ze_result_t frequencyGetState(zes_freq_state_t *pState) override;
    ze_result_t frequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) override;

    // Overclocking
    ze_result_t frequencyOcGetCapabilities(zes_oc_capabilities_t *pOcCapabilities) override;
    ze_result_t frequencyOcGetFrequencyTarget(double *pCurrentOcFrequency) override;
    ze_result_t frequencyOcSetFrequencyTarget(double currentOcFrequency) override;
    ze_result_t frequencyOcGetVoltageTarget(double *pCurrentVoltageTarget, double *pCurrentVoltageOffset) override;
    ze_result_t frequencyOcSetVoltageTarget(double currentVoltageTarget, double currentVoltageOffset) override;
    ze_result_t frequencyOcGetMode(zes_oc_mode_t *pCurrentOcMode) override;
    ze_result_t frequencyOcSetMode(zes_oc_mode_t currentOcMode) override;
    ze_result_t frequencyOcGetIccMax(double *pOcIccMax) override;
    ze_result_t frequencyOcSetIccMax(double ocIccMax) override;
    ze_result_t frequencyOcGeTjMax(double *pOcTjMax) override;
    ze_result_t frequencyOcSetTjMax(double ocTjMax) override;

    FrequencyImp() = default;
    FrequencyImp(OsSysman *pOsSysman, ze_device_handle_t handle, zes_freq_domain_t frequencyDomainNumber);
    ~FrequencyImp() override;
    OsFrequency *pOsFrequency = nullptr;
    void init();

  private:
    zes_freq_properties_t zesFrequencyProperties = {};
    double *pClocks = nullptr;
    uint32_t numClocks = 0;
    ze_device_handle_t deviceHandle = nullptr;
};

} // namespace L0
