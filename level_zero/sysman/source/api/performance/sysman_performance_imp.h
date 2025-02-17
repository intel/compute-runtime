/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/performance/sysman_os_performance.h"
#include "level_zero/sysman/source/api/performance/sysman_performance.h"
#include <level_zero/zes_api.h>

namespace L0 {
namespace Sysman {

class PerformanceImp : public Performance, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t performanceGetProperties(zes_perf_properties_t *pProperties) override;
    ze_result_t performanceGetConfig(double *pFactor) override;
    ze_result_t performanceSetConfig(double pFactor) override;

    PerformanceImp() = delete;
    PerformanceImp(OsSysman *pOsSysman, bool onSubdevice, uint32_t subDeviceId, zes_engine_type_flag_t domain);
    ~PerformanceImp() override;
    OsPerformance *pOsPerformance = nullptr;

    void init();

  private:
    zes_perf_properties_t performanceProperties = {};
};

} // namespace Sysman
} // namespace L0
