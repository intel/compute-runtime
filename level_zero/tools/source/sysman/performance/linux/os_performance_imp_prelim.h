/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/performance/os_performance.h"
#include "level_zero/tools/source/sysman/performance/performance_imp.h"

namespace L0 {

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
    SysfsAccess *pSysfsAccess = nullptr;
    Device *pDevice = nullptr;
    zes_engine_type_flag_t domain = ZES_ENGINE_TYPE_FLAG_OTHER;

  private:
    std::string mediaFreqFactor;
    std::string baseFreqFactor;
    static const std::string sysPwrBalance;
    std::string baseScale;
    std::string mediaScale;
    uint32_t subdeviceId = 0;
    ze_bool_t isSubdevice = 0;
    double baseScaleReading = 0;
    double mediaScaleReading = 0;
    ze_result_t getMediaFreqFactor();
    ze_result_t getMediaScaleFactor();
    ze_result_t getBaseFreqFactor();
    ze_result_t getBaseScaleFactor();
    void init();
    void getMultiplierVal(double rp0Reading, double rpnReading, double pFactor, double &multiplier);
    void getPerformanceFactor(double rp0Reading, double rpnReading, double multiplierReading, double *pFactor);
    ze_result_t getErrorCode(ze_result_t result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
};

} // namespace L0
