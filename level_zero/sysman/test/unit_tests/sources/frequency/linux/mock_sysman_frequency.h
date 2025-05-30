/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/sysman/source/api/frequency/linux/sysman_os_frequency_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

#include "gtest/gtest.h"
#include "neo_igfxfmid.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::string minFreqFile("gt/gt0/rps_min_freq_mhz");
const std::string minDefaultFreqFile("gt/gt0/.defaults/rps_min_freq_mhz");
const std::string maxFreqFile("gt/gt0/rps_max_freq_mhz");
const std::string boostFreqFile("gt/gt0/rps_boost_freq_mhz");
const std::string maxDefaultFreqFile("gt/gt0/.defaults/rps_max_freq_mhz");
const std::string requestFreqFile("gt/gt0/punit_req_freq_mhz");
const std::string tdpFreqFile("gt/gt0/rapl_PL1_freq_mhz");
const std::string actualFreqFile("gt/gt0/rps_act_freq_mhz");
const std::string efficientFreqFile("gt/gt0/rps_RP1_freq_mhz");
const std::string maxValFreqFile("gt/gt0/rps_RP0_freq_mhz");
const std::string minValFreqFile("gt/gt0/rps_RPn_freq_mhz");
const std::string throttleReasonStatusFile("gt/gt0/throttle_reason_status");
const std::string throttleReasonPL1File("gt/gt0/throttle_reason_pl1");
const std::string throttleReasonPL2File("gt/gt0/throttle_reason_pl2");
const std::string throttleReasonPL4File("gt/gt0/throttle_reason_pl4");
const std::string throttleReasonThermalFile("gt/gt0/throttle_reason_thermal");

const std::string minFreqFileLegacy("gt_min_freq_mhz");
const std::string maxFreqFileLegacy("gt_max_freq_mhz");
const std::string boostFreqFileLegacy("gt_boost_freq_mhz");
const std::string requestFreqFileLegacy("gt_cur_freq_mhz");
const std::string tdpFreqFileLegacy("rapl_PL1_freq_mhz");
const std::string actualFreqFileLegacy("gt_act_freq_mhz");
const std::string efficientFreqFileLegacy("gt_RP1_freq_mhz");
const std::string maxValFreqFileLegacy("gt_RP0_freq_mhz");
const std::string minValFreqFileLegacy("gt_RPn_freq_mhz");
const std::string throttleReasonStatusFileLegacy("gt_throttle_reason_status");
const std::string throttleReasonPL1FileLegacy("gt_throttle_reason_status_pl1");
const std::string throttleReasonPL2FileLegacy("gt_throttle_reason_status_pl2");
const std::string throttleReasonPL4FileLegacy("gt_throttle_reason_status_pl4");
const std::string throttleReasonThermalFileLegacy("gt_throttle_reason_status_thermal");

struct MockFrequencySysfsAccess : public L0::Sysman::SysFsAccessInterface {
    double mockMin = 0;
    double mockMax = 0;
    double mockBoost = 0;
    double mockRequest = 0;
    double mockDefaultMin = 1;
    double mockDefaultMax = 1000;
    double mockTdp = 0;
    double mockActual = 0;
    double mockEfficient = 0;
    double mockMaxVal = 0;
    double mockMinVal = 0;
    uint32_t throttleVal = 0;
    uint32_t throttleReasonPL1Val = 0;
    uint32_t throttleReasonPL2Val = 0;
    uint32_t throttleReasonPL4Val = 0;
    uint32_t throttleReasonThermalVal = 0;
    ze_result_t mockReadDoubleValResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadUnsignedIntResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadRequestResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadTdpResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadEfficientResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadActualResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadMinValResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadMaxValResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadMaxResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadPL1Result = ZE_RESULT_SUCCESS;
    ze_result_t mockReadPL2Result = ZE_RESULT_SUCCESS;
    ze_result_t mockReadPL4Result = ZE_RESULT_SUCCESS;
    ze_result_t mockReadThermalResult = ZE_RESULT_SUCCESS;
    ze_result_t mockWriteMaxResult = ZE_RESULT_SUCCESS;
    ze_result_t mockWriteMinResult = ZE_RESULT_SUCCESS;
    ze_bool_t isLegacy = false;

    ADDMETHOD_NOBASE(directoryExists, bool, true, (const std::string path));

    ze_result_t read(const std::string file, double &val) override {
        if (mockReadDoubleValResult != ZE_RESULT_SUCCESS) {
            return mockReadDoubleValResult;
        }

        if (isLegacy) {
            return getValLegacy(file, val);
        }

        if (file.compare(minFreqFile) == 0) {
            val = mockMin;
        } else if (file.compare(maxFreqFile) == 0) {
            if (mockReadMaxResult != ZE_RESULT_SUCCESS) {
                return mockReadMaxResult;
            }
            val = mockMax;
        } else if (file.compare(requestFreqFile) == 0) {
            if (mockReadRequestResult != ZE_RESULT_SUCCESS) {
                return mockReadRequestResult;
            }
            val = mockRequest;
        } else if (file.compare(tdpFreqFile) == 0) {
            if (mockReadTdpResult != ZE_RESULT_SUCCESS) {
                return mockReadTdpResult;
            }
            val = mockTdp;
        } else if (file.compare(actualFreqFile) == 0) {
            if (mockReadActualResult != ZE_RESULT_SUCCESS) {
                return mockReadActualResult;
            }
            val = mockActual;
        } else if (file.compare(efficientFreqFile) == 0) {
            if (mockReadEfficientResult != ZE_RESULT_SUCCESS) {
                return mockReadEfficientResult;
            }
            val = mockEfficient;
        } else if (file.compare(maxValFreqFile) == 0) {
            if (mockReadMaxValResult != ZE_RESULT_SUCCESS) {
                return mockReadMaxValResult;
            }
            val = mockMaxVal;
        } else if (file.compare(minValFreqFile) == 0) {
            if (mockReadMinValResult != ZE_RESULT_SUCCESS) {
                return mockReadMinValResult;
            }
            val = mockMinVal;
        } else if (file.compare(minDefaultFreqFile) == 0) {
            val = mockDefaultMin;
        } else if (file.compare(maxDefaultFreqFile) == 0) {
            val = mockDefaultMax;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t read(const std::string file, uint32_t &val) override {
        if (mockReadUnsignedIntResult != ZE_RESULT_SUCCESS) {
            return mockReadUnsignedIntResult;
        }

        if (isLegacy) {
            return getValU32Legacy(file, val);
        }
        if (file.compare(throttleReasonStatusFile) == 0) {
            val = throttleVal;
        }
        if (file.compare(throttleReasonPL1File) == 0) {
            if (mockReadPL1Result != ZE_RESULT_SUCCESS) {
                return mockReadPL1Result;
            }
            val = throttleReasonPL1Val;
        }
        if (file.compare(throttleReasonPL2File) == 0) {
            if (mockReadPL2Result != ZE_RESULT_SUCCESS) {
                return mockReadPL2Result;
            }
            val = throttleReasonPL2Val;
        }
        if (file.compare(throttleReasonPL4File) == 0) {
            if (mockReadPL4Result != ZE_RESULT_SUCCESS) {
                return mockReadPL4Result;
            }
            val = throttleReasonPL4Val;
        }
        if (file.compare(throttleReasonThermalFile) == 0) {
            if (mockReadThermalResult != ZE_RESULT_SUCCESS) {
                return mockReadThermalResult;
            }
            val = throttleReasonThermalVal;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValLegacy(const std::string file, double &val) {
        if (file.compare(minFreqFileLegacy) == 0) {
            val = mockMin;
        }
        if (file.compare(maxFreqFileLegacy) == 0) {
            val = mockMax;
        }
        if (file.compare(requestFreqFileLegacy) == 0) {
            val = mockRequest;
        }
        if (file.compare(tdpFreqFileLegacy) == 0) {
            val = mockTdp;
        }
        if (file.compare(actualFreqFileLegacy) == 0) {
            val = mockActual;
        }
        if (file.compare(efficientFreqFileLegacy) == 0) {
            val = mockEfficient;
        }
        if (file.compare(maxValFreqFileLegacy) == 0) {
            val = mockMaxVal;
        }
        if (file.compare(minValFreqFileLegacy) == 0) {
            val = mockMinVal;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValU32Legacy(const std::string file, uint32_t &val) {
        if (file.compare(throttleReasonStatusFileLegacy) == 0) {
            val = throttleVal;
        }
        if (file.compare(throttleReasonPL1FileLegacy) == 0) {
            val = throttleReasonPL1Val;
        }
        if (file.compare(throttleReasonPL2FileLegacy) == 0) {
            val = throttleReasonPL2Val;
        }
        if (file.compare(throttleReasonPL4FileLegacy) == 0) {
            val = throttleReasonPL4Val;
        }
        if (file.compare(throttleReasonThermalFileLegacy) == 0) {
            val = throttleReasonThermalVal;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setVal(const std::string file, const double val) {
        if (file.compare(minFreqFile) == 0) {
            if (mockWriteMinResult != ZE_RESULT_SUCCESS) {
                return mockWriteMinResult;
            }
            mockMin = val;
        }
        if (file.compare(maxFreqFile) == 0) {
            if (mockWriteMaxResult != ZE_RESULT_SUCCESS) {
                return mockWriteMaxResult;
            }
            mockMax = val;
        }
        if (file.compare(boostFreqFile) == 0) {
            mockBoost = val;
        }
        if (file.compare(requestFreqFile) == 0) {
            mockRequest = val;
        }
        if (file.compare(tdpFreqFile) == 0) {
            mockTdp = val;
        }
        if (file.compare(actualFreqFile) == 0) {
            mockActual = val;
        }
        if (file.compare(efficientFreqFile) == 0) {
            mockEfficient = val;
        }
        if (file.compare(maxValFreqFile) == 0) {
            mockMaxVal = val;
        }
        if (file.compare(minValFreqFile) == 0) {
            mockMinVal = val;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setValU32(const std::string file, uint32_t val) {
        if (file.compare(throttleReasonStatusFile) == 0) {
            throttleVal = val;
        }
        if (file.compare(throttleReasonPL1File) == 0) {
            throttleReasonPL1Val = val;
        }
        if (file.compare(throttleReasonPL2File) == 0) {
            throttleReasonPL2Val = val;
        }
        if (file.compare(throttleReasonPL4File) == 0) {
            throttleReasonPL4Val = val;
        }
        if (file.compare(throttleReasonThermalFile) == 0) {
            throttleReasonThermalVal = val;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setValLegacy(const std::string file, const double val) {
        if (file.compare(minFreqFileLegacy) == 0) {
            mockMin = val;
        } else if (file.compare(maxFreqFileLegacy) == 0) {
            mockMax = val;
        } else if (file.compare(boostFreqFileLegacy) == 0) {
            mockBoost = val;
        } else if (file.compare(requestFreqFileLegacy) == 0) {
            mockRequest = val;
        } else if (file.compare(tdpFreqFileLegacy) == 0) {
            mockTdp = val;
        } else if (file.compare(actualFreqFileLegacy) == 0) {
            mockActual = val;
        } else if (file.compare(efficientFreqFileLegacy) == 0) {
            mockEfficient = val;
        } else if (file.compare(maxValFreqFileLegacy) == 0) {
            mockMaxVal = val;
        } else if (file.compare(minValFreqFileLegacy) == 0) {
            mockMinVal = val;
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setValU32Legacy(const std::string file, uint32_t val) {
        if (file.compare(throttleReasonStatusFileLegacy) == 0) {
            throttleVal = val;
        }
        if (file.compare(throttleReasonPL1FileLegacy) == 0) {
            throttleReasonPL1Val = val;
        }
        if (file.compare(throttleReasonPL2FileLegacy) == 0) {
            throttleReasonPL2Val = val;
        }
        if (file.compare(throttleReasonPL4FileLegacy) == 0) {
            throttleReasonPL4Val = val;
        }
        if (file.compare(throttleReasonThermalFileLegacy) == 0) {
            throttleReasonThermalVal = val;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t write(const std::string file, double val) override {
        if (isLegacy) {
            return setValLegacy(file, val);
        }
        return setVal(file, val);
    }

    MockFrequencySysfsAccess() = default;
    ~MockFrequencySysfsAccess() override = default;
};

class PublicLinuxFrequencyImp : public L0::Sysman::LinuxFrequencyImp {
  public:
    PublicLinuxFrequencyImp(L0::Sysman::OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t type) : L0::Sysman::LinuxFrequencyImp(pOsSysman, onSubdevice, subdeviceId, type) {}
    using L0::Sysman::LinuxFrequencyImp::getMaxVal;
    using L0::Sysman::LinuxFrequencyImp::getMin;
    using L0::Sysman::LinuxFrequencyImp::getMinVal;
    using L0::Sysman::LinuxFrequencyImp::pSysfsAccess;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
