/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/frequency/os_frequency.h"

namespace L0 {

class WddmFrequencyImp : public OsFrequency {
  public:
    ze_result_t osFrequencyGetProperties(zes_freq_properties_t &properties) override;
    ze_result_t osFrequencyGetRange(zes_freq_range_t *pLimits) override;
    ze_result_t osFrequencySetRange(const zes_freq_range_t *pLimits) override;
    ze_result_t osFrequencyGetState(zes_freq_state_t *pState) override;
    ze_result_t osFrequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) override;
};

ze_result_t WddmFrequencyImp::osFrequencyGetProperties(zes_freq_properties_t &properties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::osFrequencyGetRange(zes_freq_range_t *pLimits) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::osFrequencySetRange(const zes_freq_range_t *pLimits) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::osFrequencyGetState(zes_freq_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::osFrequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

OsFrequency *OsFrequency::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    WddmFrequencyImp *pWddmFrequencyImp = new WddmFrequencyImp();
    return static_cast<OsFrequency *>(pWddmFrequencyImp);
}

uint16_t OsFrequency::getHardwareBlockCount(ze_device_handle_t handle) {
    return 1;
}

} // namespace L0
