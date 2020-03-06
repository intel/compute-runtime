/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "../os_frequency.h"

namespace L0 {

class WddmFrequencyImp : public OsFrequency {
  public:
    ze_result_t getMin(double &min) override;
    ze_result_t setMin(double min) override;
    ze_result_t getMax(double &max) override;
    ze_result_t setMax(double max) override;
    ze_result_t getRequest(double &request) override;
    ze_result_t getTdp(double &tdp) override;
    ze_result_t getActual(double &actual) override;
    ze_result_t getEfficient(double &efficient) override;
    ze_result_t getMaxVal(double &maxVal) override;
    ze_result_t getMinVal(double &minVal) override;
    ze_result_t getThrottleReasons(uint32_t &throttleReasons) override;
};

ze_result_t WddmFrequencyImp::getMin(double &min) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::setMin(double min) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getMax(double &max) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::setMax(double max) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getRequest(double &request) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getTdp(double &tdp) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getActual(double &actual) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getEfficient(double &efficient) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getMaxVal(double &maxVal) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getMinVal(double &minVal) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getThrottleReasons(uint32_t &throttleReasons) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

OsFrequency *OsFrequency::create(OsSysman *pOsSysman) {
    WddmFrequencyImp *pWddmFrequencyImp = new WddmFrequencyImp();
    return static_cast<OsFrequency *>(pWddmFrequencyImp);
}

} // namespace L0
