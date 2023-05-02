/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman.h"
#include <level_zero/zes_api.h>

#include <memory>

namespace L0 {

class OsPerformance {
  public:
    virtual ze_result_t osPerformanceGetProperties(zes_perf_properties_t &pProperties) = 0;
    virtual ze_result_t osPerformanceGetConfig(double *pFactor) = 0;
    virtual ze_result_t osPerformanceSetConfig(double pFactor) = 0;

    virtual bool isPerformanceSupported(void) = 0;

    static std::unique_ptr<OsPerformance> create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_engine_type_flag_t domain);
    virtual ~OsPerformance() {}
};

} // namespace L0
