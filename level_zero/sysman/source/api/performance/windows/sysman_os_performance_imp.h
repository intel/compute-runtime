/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/sysman/source/api/performance/sysman_os_performance.h"
#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

class WddmPerformanceImp : public OsPerformance {
  public:
    ze_result_t osPerformanceGetProperties(zes_perf_properties_t &pProperties) override;
    ze_result_t osPerformanceGetConfig(double *pFactor) override;
    ze_result_t osPerformanceSetConfig(double pFactor) override;
    bool isPerformanceSupported(void) override;
    WddmPerformanceImp() = default;
    WddmPerformanceImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId,
                       zes_engine_type_flag_t domain);
    ~WddmPerformanceImp() override = default;

  protected:
    ze_bool_t isSubdevice = false;
    uint32_t subDeviceId = 0;
    KmdSysManager *pKmdSysManager = nullptr;
    zes_engine_type_flag_t domain = ZES_ENGINE_TYPE_FLAG_OTHER;
};

} // namespace Sysman
} // namespace L0
