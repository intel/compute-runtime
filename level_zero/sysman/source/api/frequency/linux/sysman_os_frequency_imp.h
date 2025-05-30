/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/frequency/sysman_frequency_imp.h"
#include "level_zero/sysman/source/api/frequency/sysman_os_frequency.h"

#include "neo_igfxfmid.h"
namespace L0 {
namespace Sysman {

class LinuxSysmanImp;
class SysmanKmdInterface;
class SysmanProductHelper;
class SysFsAccessInterface;

class LinuxFrequencyImp : public OsFrequency, NEO::NonCopyableAndNonMovableClass {
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
    LinuxFrequencyImp() = default;
    LinuxFrequencyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t frequencyDomainNumber);
    ~LinuxFrequencyImp() override = default;

  protected:
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    SysmanKmdInterface *pSysmanKmdInterface = nullptr;
    SysFsAccessInterface *pSysfsAccess = nullptr;
    ze_result_t getMin(double &min);
    ze_result_t setMin(double min);
    ze_result_t getMax(double &max);
    ze_result_t setMax(double max);
    ze_result_t getRequest(double &request);
    ze_result_t getTdp(double &tdp);
    ze_result_t getActual(double &actual);
    ze_result_t getEfficient(double &efficient);
    ze_result_t getMaxVal(double &maxVal);
    ze_result_t getMinVal(double &minVal);
    void getCurrentVoltage(double &voltage);

  private:
    std::string minFreqFile;
    std::string maxFreqFile;
    std::string boostFreqFile;
    std::string minDefaultFreqFile;
    std::string maxDefaultFreqFile;
    std::string requestFreqFile;
    std::string tdpFreqFile;
    std::string actualFreqFile;
    std::string efficientFreqFile;
    std::string maxValFreqFile;
    std::string minValFreqFile;
    bool canControl = false;
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
    zes_freq_domain_t frequencyDomainNumber = ZES_FREQ_DOMAIN_GPU;
    SysmanProductHelper *pSysmanProductHelper = nullptr;
    void init();
};

} // namespace Sysman
} // namespace L0
