/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <level_zero/zes_api.h>

#include "os_performance.h"
#include "performance.h"

namespace L0 {

class PerformanceImp : public Performance, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t performanceGetProperties(zes_perf_properties_t *pProperties) override;
    ze_result_t performanceGetConfig(double *pFactor) override;
    ze_result_t performanceSetConfig(double pFactor) override;

    PerformanceImp() = delete;
    PerformanceImp(OsSysman *pOsSysman, ze_device_handle_t handle, zes_engine_type_flag_t domain);
    ~PerformanceImp() override;
    OsPerformance *pOsPerformance = nullptr;

    void init();

  private:
    zes_perf_properties_t performanceProperties = {};
};

} // namespace L0
