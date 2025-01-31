/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/sysman/source/api/frequency/linux/sysman_os_frequency_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::string minFreqFile("device/tile0/gt0/freq0/min_freq");
const std::string maxFreqFile("device/tile0/gt0/freq0/max_freq");
const std::string requestFreqFile("device/tile0/gt0/freq0/cur_freq");
const std::string actualFreqFile("device/tile0/gt0/freq0/act_freq");
const std::string efficientFreqFile("device/tile0/gt0/freq0/rpe_freq");
const std::string maxValFreqFile("device/tile0/gt0/freq0/rp0_freq");
const std::string minValFreqFile("device/tile0/gt0/freq0/rpn_freq");
const std::string throttleReasonStatusFile("device/tile0/gt0/freq0/throttle/status");
const std::string throttleReasonPL1File("device/tile0/gt0/freq0/throttle/reason_pl1");
const std::string throttleReasonPL2File("device/tile0/gt0/freq0/throttle/reason_pl2");
const std::string throttleReasonPL4File("device/tile0/gt0/freq0/throttle/reason_pl4");
const std::string throttleReasonThermalFile("device/tile0/gt0/freq0/throttle/reason_thermal");
const std::string throttleReasonFile("device/tile0/gt0/freq0/throttle_reason");

struct MockXeFrequencySysfsAccess : public L0::Sysman::SysFsAccessInterface {
    std::string throttleReason = {};
    double mockMin = 0;
    double mockMax = 0;
    double mockRequest = 0;
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
    ze_result_t mockReadRequestResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadEfficientResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadActualResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadMinValResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadMaxValResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadMaxResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadVal32Result = ZE_RESULT_SUCCESS;
    ze_result_t mockWriteMaxResult = ZE_RESULT_SUCCESS;
    ze_result_t mockWriteMinResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadStringResult = ZE_RESULT_SUCCESS;
    bool mockReadPL1Error = false;
    bool mockReadPL2Error = false;
    bool mockReadPL4Error = false;
    bool mockReadThermalError = false;

    ADDMETHOD_NOBASE(directoryExists, bool, true, (const std::string path));

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

    ze_result_t read(const std::string file, double &val) override {

        if (mockReadDoubleValResult != ZE_RESULT_SUCCESS) {
            return mockReadDoubleValResult;
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
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
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
        if (file.compare(requestFreqFile) == 0) {
            mockRequest = val;
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
        if (file.compare(throttleReasonStatusFile) == 0) {
            val = throttleVal;
        }
        if (file.compare(throttleReasonPL1File) == 0) {
            if (mockReadPL1Error) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = throttleReasonPL1Val;
        }
        if (file.compare(throttleReasonPL2File) == 0) {
            if (mockReadPL2Error) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = throttleReasonPL2Val;
        }
        if (file.compare(throttleReasonPL4File) == 0) {
            if (mockReadPL4Error) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = throttleReasonPL4Val;
        }
        if (file.compare(throttleReasonThermalFile) == 0) {
            if (mockReadThermalError) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = throttleReasonThermalVal;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t read(const std::string file, uint32_t &val) override {
        if (mockReadVal32Result != ZE_RESULT_SUCCESS) {
            return mockReadVal32Result;
        }
        return getValU32(file, val);
    }

    ze_result_t write(const std::string file, double val) override {
        return setVal(file, val);
    }

    ze_result_t read(const std::string file, std::string &val) override {

        if (mockReadStringResult != ZE_RESULT_SUCCESS) {
            return mockReadStringResult;
        }

        if (file.compare(throttleReasonFile) == 0) {
            val = throttleReason;
        }

        return ZE_RESULT_SUCCESS;
    }

    MockXeFrequencySysfsAccess() = default;
    ~MockXeFrequencySysfsAccess() override = default;
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
