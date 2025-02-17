/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/performance/sysman_os_performance.h"
#include "level_zero/sysman/source/api/performance/sysman_performance_imp.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

class SysmanKmdInterface;
class SysFsAccessInterface;
class LinuxPerformanceImp : public OsPerformance, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t osPerformanceGetProperties(zes_perf_properties_t &pProperties) override;
    ze_result_t osPerformanceGetConfig(double *pFactor) override;
    ze_result_t osPerformanceSetConfig(double pFactor) override;

    bool isPerformanceSupported(void) override;

    LinuxPerformanceImp() = delete;
    LinuxPerformanceImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId,
                        zes_engine_type_flag_t domain);
    ~LinuxPerformanceImp() override = default;

  protected:
    ze_result_t getMediaFrequencyScaleFactor();
    ze_result_t getBaseFrequencyScaleFactor();
    ze_result_t getErrorCode(ze_result_t result);
    SysmanKmdInterface *pSysmanKmdInterface = nullptr;
    SysFsAccessInterface *pSysFsAccess = nullptr;
    SysmanProductHelper *pSysmanProductHelper = nullptr;
    zes_engine_type_flag_t domain = ZES_ENGINE_TYPE_FLAG_OTHER;
    double baseScaleReading = 0;
    double mediaScaleReading = 0;
    uint32_t subdeviceId = 0;
    ze_bool_t isSubdevice = 0;
    std::string mediaFreqFactor = "";
    std::string baseFreqFactor = "";
    std::string systemPowerBalance = "";
    std::string baseFreqFactorScale = "";
    std::string mediaFreqFactorScale = "";
};

} // namespace Sysman
} // namespace L0
