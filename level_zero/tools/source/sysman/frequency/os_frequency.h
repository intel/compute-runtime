/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman.h"
#include <level_zero/zet_api.h>

namespace L0 {

class OsFrequency {
  public:
    virtual ze_result_t getMin(double &min) = 0;
    virtual ze_result_t setMin(double min) = 0;
    virtual ze_result_t getMax(double &max) = 0;
    virtual ze_result_t setMax(double max) = 0;
    virtual ze_result_t getRequest(double &request) = 0;
    virtual ze_result_t getTdp(double &tdp) = 0;
    virtual ze_result_t getActual(double &actual) = 0;
    virtual ze_result_t getEfficient(double &efficient) = 0;
    virtual ze_result_t getMaxVal(double &maxVal) = 0;
    virtual ze_result_t getMinVal(double &minVal) = 0;
    virtual ze_result_t getThrottleReasons(uint32_t &throttleReasons) = 0;

    static OsFrequency *create(OsSysman *pOsSysman);
    virtual ~OsFrequency() {}
};

} // namespace L0
