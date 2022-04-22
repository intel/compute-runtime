/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/frequency/linux/os_frequency_imp.h"

namespace L0 {
namespace ult {

const std::string minFreqFile("gt/gt0/rps_min_freq_mhz");
const std::string maxFreqFile("gt/gt0/rps_max_freq_mhz");
const std::string boostFreqFile("gt/gt0/rps_boost_freq_mhz");
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

class FrequencySysfsAccess : public SysfsAccess {};

template <>
struct Mock<FrequencySysfsAccess> : public FrequencySysfsAccess {
    double mockMin = 0;
    double mockMax = 0;
    double mockBoost = 0;
    double mockRequest = 0;
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
    ze_result_t mockReadVal32Result = ZE_RESULT_SUCCESS;
    bool mockReadPL1Error = false;
    bool mockReadPL2Error = false;
    bool mockReadPL4Error = false;
    bool mockReadThermalError = false;

    MOCK_METHOD(ze_result_t, read, (const std::string file, double &val), (override));
    MOCK_METHOD(ze_result_t, write, (const std::string file, const double val), (override));
    MOCK_METHOD(bool, directoryExists, (const std::string path), (override));

    bool mockDirectoryExistsSuccess(const std::string path) {
        return true;
    }

    bool mockDirectoryExistsFailure(const std::string path) {
        return false;
    }

    ze_result_t getMaxValReturnErrorNotAvailable(const std::string file, double &val) {
        if (file.compare(maxValFreqFile) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getMaxValReturnErrorUnknown(const std::string file, double &val) {
        if (file.compare(maxValFreqFile) == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getMinValReturnErrorNotAvailable(const std::string file, double &val) {
        if (file.compare(minValFreqFile) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getMinValReturnErrorUnknown(const std::string file, double &val) {
        if (file.compare(minValFreqFile) == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValReturnErrorNotAvailable(const std::string file, double &val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getValActualReturnErrorNotAvailable(const std::string file, double &val) {
        if (file.compare(actualFreqFile) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValEfficientReturnErrorNotAvailable(const std::string file, double &val) {
        if (file.compare(efficientFreqFile) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValTdpReturnErrorNotAvailable(const std::string file, double &val) {
        if (file.compare(tdpFreqFile) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValRequestReturnErrorNotAvailable(const std::string file, double &val) {
        if (file.compare(requestFreqFile) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setValMinReturnErrorNotAvailable(const std::string file, const double val) {
        if (file.compare(minFreqFile) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setValMaxReturnErrorNotAvailable(const std::string file, const double val) {
        if (file.compare(maxFreqFile) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValReturnErrorUnknown(const std::string file, double &val) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t getValActualReturnErrorUnknown(const std::string file, double &val) {
        if (file.compare(actualFreqFile) == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValEfficientReturnErrorUnknown(const std::string file, double &val) {
        if (file.compare(efficientFreqFile) == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValTdpReturnErrorUnknown(const std::string file, double &val) {
        if (file.compare(tdpFreqFile) == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValRequestReturnErrorUnknown(const std::string file, double &val) {
        if (file.compare(requestFreqFile) == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setValMinReturnErrorUnknown(const std::string file, const double val) {
        if (file.compare(minFreqFile) == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setValMaxReturnErrorUnknown(const std::string file, const double val) {
        if (file.compare(maxFreqFile) == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
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

    ze_result_t getVal(const std::string file, double &val) {
        if (file.compare(minFreqFile) == 0) {
            val = mockMin;
        }
        if (file.compare(maxFreqFile) == 0) {
            val = mockMax;
        }
        if (file.compare(requestFreqFile) == 0) {
            val = mockRequest;
        }
        if (file.compare(tdpFreqFile) == 0) {
            val = mockTdp;
        }
        if (file.compare(actualFreqFile) == 0) {
            val = mockActual;
        }
        if (file.compare(efficientFreqFile) == 0) {
            val = mockEfficient;
        }
        if (file.compare(maxValFreqFile) == 0) {
            val = mockMaxVal;
        }
        if (file.compare(minValFreqFile) == 0) {
            val = mockMinVal;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setVal(const std::string file, const double val) {
        if (file.compare(minFreqFile) == 0) {
            mockMin = val;
        }
        if (file.compare(maxFreqFile) == 0) {
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

    ze_result_t getValU32(const std::string file, uint32_t &val) {
        if ((file.compare(throttleReasonStatusFile) == 0) || (file.compare(throttleReasonStatusFileLegacy) == 0)) {
            val = throttleVal;
        }
        if ((file.compare(throttleReasonPL1File) == 0) || (file.compare(throttleReasonPL1FileLegacy) == 0)) {
            if (mockReadPL1Error) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = throttleReasonPL1Val;
        }
        if ((file.compare(throttleReasonPL2File) == 0) || (file.compare(throttleReasonPL2FileLegacy) == 0)) {
            if (mockReadPL2Error) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = throttleReasonPL2Val;
        }
        if ((file.compare(throttleReasonPL4File) == 0) || (file.compare(throttleReasonPL4FileLegacy) == 0)) {
            if (mockReadPL4Error) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = throttleReasonPL4Val;
        }
        if ((file.compare(throttleReasonThermalFile) == 0) || (file.compare(throttleReasonThermalFileLegacy) == 0)) {
            if (mockReadThermalError) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = throttleReasonThermalVal;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValU32Error(const std::string file, uint32_t &val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, uint32_t &val) override {
        if (mockReadVal32Result != ZE_RESULT_SUCCESS) {
            return mockReadVal32Result;
        }
        return getValU32(file, val);
    }

    Mock() = default;
    ~Mock() override = default;
};

class PublicLinuxFrequencyImp : public L0::LinuxFrequencyImp {
  public:
    PublicLinuxFrequencyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t type) : LinuxFrequencyImp(pOsSysman, onSubdevice, subdeviceId, type) {}
    using LinuxFrequencyImp::getMaxVal;
    using LinuxFrequencyImp::getMin;
    using LinuxFrequencyImp::getMinVal;
    using LinuxFrequencyImp::pSysfsAccess;
};

} // namespace ult
} // namespace L0
