/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/device/os_sysman.h"
#include <level_zero/zes_api.h>

namespace L0 {
namespace Sysman {

class OsPerformance {
  public:
    virtual ze_result_t osPerformanceGetProperties(zes_perf_properties_t &pProperties) = 0;
    virtual ze_result_t osPerformanceGetConfig(double *pFactor) = 0;
    virtual ze_result_t osPerformanceSetConfig(double pFactor) = 0;

    virtual bool isPerformanceSupported(void) = 0;

    static OsPerformance *create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_engine_type_flag_t domain);
    virtual ~OsPerformance() {}
};

} // namespace Sysman
} // namespace L0
