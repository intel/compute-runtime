/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "level_zero/core/source/device/device.h"

#include "sysman/frequency/frequency_imp.h"
#include "sysman/frequency/os_frequency.h"
#include "sysman/linux/fs_access.h"
#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

class LinuxFrequencyImp : public OsFrequency {
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

    LinuxFrequencyImp(OsSysman *pOsSysman);
    ~LinuxFrequencyImp() override = default;

    // Don't allow copies of the LinuxFrequencyImp object
    LinuxFrequencyImp(const LinuxFrequencyImp &obj) = delete;
    LinuxFrequencyImp &operator=(const LinuxFrequencyImp &obj) = delete;

  private:
    SysfsAccess *pSysfsAccess;

    static const std::string minFreqFile;
    static const std::string maxFreqFile;
    static const std::string requestFreqFile;
    static const std::string tdpFreqFile;
    static const std::string actualFreqFile;
    static const std::string efficientFreqFile;
    static const std::string maxValFreqFile;
    static const std::string minValFreqFile;
};

const std::string LinuxFrequencyImp::minFreqFile("gt_min_freq_mhz");
const std::string LinuxFrequencyImp::maxFreqFile("gt_max_freq_mhz");
const std::string LinuxFrequencyImp::requestFreqFile("gt_cur_freq_mhz");
const std::string LinuxFrequencyImp::tdpFreqFile("gt_boost_freq_mhz");
const std::string LinuxFrequencyImp::actualFreqFile("gt_act_freq_mhz");
const std::string LinuxFrequencyImp::efficientFreqFile("gt_RP1_freq_mhz");
const std::string LinuxFrequencyImp::maxValFreqFile("gt_RP0_freq_mhz");
const std::string LinuxFrequencyImp::minValFreqFile("gt_RPn_freq_mhz");

ze_result_t LinuxFrequencyImp::getMin(double &min) {
    int intval;

    ze_result_t result = pSysfsAccess->read(minFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    min = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::setMin(double min) {
    ze_result_t result = pSysfsAccess->write(minFreqFile, min);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getMax(double &max) {
    int intval;

    ze_result_t result = pSysfsAccess->read(maxFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    max = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::setMax(double max) {
    ze_result_t result = pSysfsAccess->write(maxFreqFile, max);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getRequest(double &request) {
    int intval;

    ze_result_t result = pSysfsAccess->read(requestFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    request = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getTdp(double &tdp) {
    int intval;

    ze_result_t result = pSysfsAccess->read(tdpFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    tdp = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getActual(double &actual) {
    int intval;

    ze_result_t result = pSysfsAccess->read(actualFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    actual = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getEfficient(double &efficient) {
    int intval;

    ze_result_t result = pSysfsAccess->read(efficientFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    efficient = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getMaxVal(double &maxVal) {
    int intval;

    ze_result_t result = pSysfsAccess->read(maxValFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    maxVal = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getMinVal(double &minVal) {
    int intval;

    ze_result_t result = pSysfsAccess->read(minValFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    minVal = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getThrottleReasons(uint32_t &throttleReasons) {
    throttleReasons = ZET_FREQ_THROTTLE_REASONS_NONE;
    return ZE_RESULT_SUCCESS;
}

LinuxFrequencyImp::LinuxFrequencyImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);

    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsFrequency *OsFrequency::create(OsSysman *pOsSysman) {
    LinuxFrequencyImp *pLinuxFrequencyImp = new LinuxFrequencyImp(pOsSysman);
    return static_cast<OsFrequency *>(pLinuxFrequencyImp);
}

} // namespace L0
