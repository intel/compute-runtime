/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/frequency/os_frequency.h"
#include "sysman/windows/os_sysman_imp.h"

#define KMD_BIT_RANGE(endbit, startbit) ((endbit) - (startbit) + 1)

namespace L0 {

struct KmdThrottleReasons {
    union {
        uint32_t bitfield;
        struct {
            uint32_t reserved1 : KMD_BIT_RANGE(16, 0);
            uint32_t thermal : KMD_BIT_RANGE(17, 17);
            uint32_t reserved2 : KMD_BIT_RANGE(23, 18);
            uint32_t powerlimit4 : KMD_BIT_RANGE(24, 24);
            uint32_t reserved3 : KMD_BIT_RANGE(25, 25);
            uint32_t powerlimit1 : KMD_BIT_RANGE(26, 26);
            uint32_t powerlimit2 : KMD_BIT_RANGE(27, 27);
            uint32_t reserved4 : KMD_BIT_RANGE(31, 28);
        };
    };
};

class KmdSysManager;
class WddmFrequencyImp : public OsFrequency, NEO::NonCopyableOrMovableClass {
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
    double minRangeFreq = -1.0;
    double maxRangeFreq = -1.0;
    zes_oc_capabilities_t ocCapabilities = {};
    zes_oc_mode_t currentVoltageMode = ZES_OC_MODE_OFF;
    double currentFrequencyTarget = -1.0;
    double currentVoltageTarget = -1.0;
    double currentVoltageOffset = -1.0;

  protected:
    KmdSysManager *pKmdSysManager = nullptr;
    zes_freq_domain_t frequencyDomainNumber = ZES_FREQ_DOMAIN_GPU;
};

} // namespace L0
