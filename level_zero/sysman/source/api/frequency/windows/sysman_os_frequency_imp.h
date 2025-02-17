/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/frequency/sysman_os_frequency.h"
#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"

#define KMD_BIT_RANGE(endbit, startbit) ((endbit) - (startbit) + 1)

namespace L0 {
namespace Sysman {

struct KmdThrottleReasons {
    union {
        uint32_t bitfield;
        struct {
            uint32_t thermal1 : KMD_BIT_RANGE(0, 0);
            uint32_t thermal2 : KMD_BIT_RANGE(1, 1);
            uint32_t reserved1 : KMD_BIT_RANGE(3, 2);
            uint32_t power1 : KMD_BIT_RANGE(4, 4);
            uint32_t power2 : KMD_BIT_RANGE(5, 5);
            uint32_t thermal3 : KMD_BIT_RANGE(6, 6);
            uint32_t thermal4 : KMD_BIT_RANGE(7, 7);
            uint32_t current1 : KMD_BIT_RANGE(8, 8);
            uint32_t reserved2 : KMD_BIT_RANGE(9, 9);
            uint32_t power3 : KMD_BIT_RANGE(10, 10);
            uint32_t power4 : KMD_BIT_RANGE(11, 11);
            uint32_t inefficient1 : KMD_BIT_RANGE(12, 12);
            uint32_t reserved3 : KMD_BIT_RANGE(13, 13);
            uint32_t inefficient2 : KMD_BIT_RANGE(14, 14);
            uint32_t reserved4 : KMD_BIT_RANGE(31, 15);
        };
    };
};

class KmdSysManager;
class WddmFrequencyImp : public OsFrequency, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t osFrequencyGetProperties(zes_freq_properties_t &properties) override;
    double osFrequencyGetStepSize() override;
    ze_result_t osFrequencyGetRange(zes_freq_range_t *pLimits) override;
    ze_result_t osFrequencySetRange(const zes_freq_range_t *pLimits) override;
    ze_result_t osFrequencyGetState(zes_freq_state_t *pState) override;
    ze_result_t osFrequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) override;

    ze_result_t getOcCapabilities(zes_oc_capabilities_t *pOcCapabilities) override;
    ze_result_t getOcFrequencyTarget(double *pCurrentOcFrequency) override;
    ze_result_t setOcFrequencyTarget(double currentOcFrequency) override;
    ze_result_t getOcVoltageTarget(double *pCurrentVoltageTarget, double *pCurrentVoltageOffset) override;
    ze_result_t setOcVoltageTarget(double currentVoltageTarget, double currentVoltageOffset) override;
    ze_result_t getOcMode(zes_oc_mode_t *pCurrentOcMode) override;
    ze_result_t setOcMode(zes_oc_mode_t currentOcMode) override;
    ze_result_t getOcIccMax(double *pOcIccMax) override;
    ze_result_t setOcIccMax(double ocIccMax) override;
    ze_result_t getOcTjMax(double *pOcTjMax) override;
    ze_result_t setOcTjMax(double ocTjMax) override;

    WddmFrequencyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t type);
    WddmFrequencyImp() = default;
    ~WddmFrequencyImp() override = default;

  private:
    ze_result_t setRange(double min, double max);
    ze_result_t getRange(double *min, double *max);
    void readOverclockingInfo();
    ze_result_t applyOcSettings();
    zes_oc_capabilities_t ocCapabilities = {};
    zes_oc_mode_t currentVoltageMode = ZES_OC_MODE_OFF;
    double currentFrequencyTarget = -1.0;
    double currentVoltageTarget = -1.0;
    double currentVoltageOffset = -1.0;

  protected:
    KmdSysManager *pKmdSysManager = nullptr;
    zes_freq_domain_t frequencyDomainNumber = ZES_FREQ_DOMAIN_GPU;
};

} // namespace Sysman
} // namespace L0
