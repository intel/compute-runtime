/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/linux/os_sysman_imp.h"
#include "sysman/performance/os_performance.h"
#include "sysman/performance/performance_imp.h"

namespace L0 {

class LinuxPerformanceImp : public OsPerformance, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t osPerformanceGetProperties(zes_perf_properties_t &pProperties) override;
    ze_result_t osPerformanceGetConfig(double *pFactor) override;
    ze_result_t osPerformanceSetConfig(double pFactor) override;

    bool isPerformanceSupported(void) override;

    LinuxPerformanceImp() = delete;
    LinuxPerformanceImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId,
                        zes_engine_type_flag_t domain) {}
    ~LinuxPerformanceImp() override = default;
};

} // namespace L0
