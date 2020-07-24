/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/frequency/frequency_imp.h"
#include "sysman/frequency/os_frequency.h"
#include "sysman/linux/fs_access.h"

namespace L0 {

class LinuxFrequencyImp : public OsFrequency, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getMin(double &min) override;
    ze_result_t setMin(double min) override;
    ze_result_t getMax(double &max) override;
    ze_result_t setMax(double max) override;
    ze_result_t getRequest(double &request) override;
    ze_result_t getTdp(double &tdp) override;
    ze_result_t getActual(double &actual) override;
    ze_result_t getEfficient(double &efficient) override;
    ze_result_t getMaxVal(double &maxVal) override;
    ze_result_t getMinVal(double &minVal) override;
    ze_result_t getThrottleReasons(uint32_t &throttleReasons) override;
    LinuxFrequencyImp() = default;
    LinuxFrequencyImp(OsSysman *pOsSysman);
    ~LinuxFrequencyImp() override = default;

  protected:
    SysfsAccess *pSysfsAccess = nullptr;

  private:
    static const std::string minFreqFile;
    static const std::string maxFreqFile;
    static const std::string requestFreqFile;
    static const std::string tdpFreqFile;
    static const std::string actualFreqFile;
    static const std::string efficientFreqFile;
    static const std::string maxValFreqFile;
    static const std::string minValFreqFile;
};

} // namespace L0
