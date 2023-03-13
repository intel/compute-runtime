/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/frequency/os_frequency.h"
#include "level_zero/sysman/source/windows/os_sysman_imp.h"

namespace L0 {
namespace Sysman {

class KmdSysManager;
class WddmFrequencyImp : public OsFrequency, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t osFrequencyGetProperties(zes_freq_properties_t &properties) override;
    double osFrequencyGetStepSize() override;
    ze_result_t osFrequencyGetRange(zes_freq_range_t *pLimits) override;
    ze_result_t osFrequencySetRange(const zes_freq_range_t *pLimits) override;
    ze_result_t osFrequencyGetState(zes_freq_state_t *pState) override;
    ze_result_t osFrequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) override;

    ze_result_t getOcCapabilities(zes_oc_capabilities_t *pOcCapabilities) override;
    ze_result_t getOcFrequencyTarget(double *pCurrentOcFrequency) override;
    ze_result_t setOcFrequencyTarget(double currentOcFrequency) override;
    ze_result_t getOcVoltageTarget(double *pCurrentVoltageTarget, double *pCurrentVoltageOffset) override;
    ze_result_t setOcVoltageTarget(double currentVoltageTarget, double currentVoltageOffset) override;
    ze_result_t getOcMode(zes_oc_mode_t *pCurrentOcMode) override;
    ze_result_t setOcMode(zes_oc_mode_t currentOcMode) override;
    ze_result_t getOcIccMax(double *pOcIccMax) override;
    ze_result_t setOcIccMax(double ocIccMax) override;
    ze_result_t getOcTjMax(double *pOcTjMax) override;
    ze_result_t setOcTjMax(double ocTjMax) override;

    WddmFrequencyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t type);
    WddmFrequencyImp() = default;
    ~WddmFrequencyImp() override = default;
};

} // namespace Sysman
} // namespace L0
