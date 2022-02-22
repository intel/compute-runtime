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
const std::string requestFreqFile("gt/gt0/punit_req_freq_mhz");
const std::string tdpFreqFile("gt/gt0/rapl_PL1_freq_mhz");
const std::string actualFreqFile("gt/gt0/rps_act_freq_mhz");
const std::string efficientFreqFile("gt/gt0/rps_RP1_freq_mhz");
const std::string maxValFreqFile("gt/gt0/rps_RP0_freq_mhz");
const std::string minValFreqFile("gt/gt0/rps_RPn_freq_mhz");

const std::string minFreqFileLegacy("gt_min_freq_mhz");
const std::string maxFreqFileLegacy("gt_max_freq_mhz");
const std::string requestFreqFileLegacy("gt_cur_freq_mhz");
const std::string tdpFreqFileLegacy("rapl_PL1_freq_mhz");
const std::string actualFreqFileLegacy("gt_act_freq_mhz");
const std::string efficientFreqFileLegacy("gt_RP1_freq_mhz");
const std::string maxValFreqFileLegacy("gt_RP0_freq_mhz");
const std::string minValFreqFileLegacy("gt_RPn_freq_mhz");

class FrequencySysfsAccess : public SysfsAccess {};

template <>
struct Mock<FrequencySysfsAccess> : public FrequencySysfsAccess {
    double mockMin = 0;
    double mockMax = 0;
    double mockRequest = 0;
    double mockTdp = 0;
    double mockActual = 0;
    double mockEfficient = 0;
    double mockMaxVal = 0;
    double mockMinVal = 0;

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
        }
        if (file.compare(maxFreqFileLegacy) == 0) {
            mockMax = val;
        }
        if (file.compare(requestFreqFileLegacy) == 0) {
            mockRequest = val;
        }
        if (file.compare(tdpFreqFileLegacy) == 0) {
            mockTdp = val;
        }
        if (file.compare(actualFreqFileLegacy) == 0) {
            mockActual = val;
        }
        if (file.compare(efficientFreqFileLegacy) == 0) {
            mockEfficient = val;
        }
        if (file.compare(maxValFreqFileLegacy) == 0) {
            mockMaxVal = val;
        }
        if (file.compare(minValFreqFileLegacy) == 0) {
            mockMinVal = val;
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
