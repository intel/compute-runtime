/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/performance/linux/os_performance_imp_prelim.h"

#include "sysman/linux/fs_access.h"
#include "sysman/performance/performance.h"
#include "sysman/performance/performance_imp.h"

namespace L0 {
namespace ult {

const std::string mediaFreqFactorSubDevice0("gt/gt0/media_freq_factor");
const std::string mediaScaleSubDevice0("gt/gt0/media_freq_factor.scale");
const std::string baseFreqFactorSubDevice0("gt/gt0/base_freq_factor");
const std::string baseScaleSubDevice0("gt/gt0/base_freq_factor.scale");
const std::string mediaFreqFactorSubDevice1("gt/gt1/media_freq_factor");
const std::string mediaScaleSubDevice1("gt/gt1/media_freq_factor.scale");
const std::string baseFreqFactorSubDevice1("gt/gt1/base_freq_factor");
const std::string baseScaleSubDevice1("gt/gt1/base_freq_factor.scale");
const std::string pwrBalance("sys_pwr_balance");
class PerformanceSysfsAccess : public SysfsAccess {};

template <>
struct Mock<PerformanceSysfsAccess> : public PerformanceSysfsAccess {
    ze_result_t mockReadResult = ZE_RESULT_SUCCESS;
    ze_result_t mockCanReadResult = ZE_RESULT_SUCCESS;
    ze_bool_t isComputeInvalid = false;
    ze_bool_t isReturnUnknownFailure = false;
    ze_bool_t returnNegativeFactor = false;
    ze_bool_t isMediaBaseFailure = false;

    ze_result_t getValWithMultiplierReturnOne(const std::string file, double &val) {
        if ((file.compare(baseFreqFactorSubDevice0) == 0) || (file.compare(baseFreqFactorSubDevice1) == 0)) {
            val = mockBaseVal1;
        } else if ((file.compare(mediaFreqFactorSubDevice0) == 0) || (file.compare(mediaFreqFactorSubDevice1) == 0)) {
            val = mockMediaVal1;
        } else if ((file.compare(mediaScaleSubDevice0) == 0) || (file.compare(mediaScaleSubDevice1) == 0) || (file.compare(baseScaleSubDevice0) == 0) || (file.compare(baseScaleSubDevice1) == 0)) {
            val = mockScale;
        } else if (file.compare(pwrBalance) == 0) {
            val = mockPwrBalance;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValUnknownFailure(const std::string file, double &val) {
        if ((file.compare(baseFreqFactorSubDevice0) == 0) || (file.compare(baseFreqFactorSubDevice1) == 0)) {
            val = 64.0;
        } else if ((file.compare(mediaFreqFactorSubDevice0) == 0) || (file.compare(mediaFreqFactorSubDevice1) == 0)) {
            val = 384.0;
        } else if ((file.compare(mediaScaleSubDevice0) == 0) || (file.compare(mediaScaleSubDevice1) == 0) || (file.compare(baseScaleSubDevice0) == 0) || (file.compare(baseScaleSubDevice1) == 0)) {
            val = mockScale;
        } else if (file.compare(pwrBalance) == 0) {
            val = 100.0;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValNegative(const std::string file, double &val) {
        const std::list<std::string> nodeList = {baseFreqFactorSubDevice0, baseFreqFactorSubDevice1, mediaFreqFactorSubDevice0, mediaFreqFactorSubDevice1, mediaScaleSubDevice0, mediaScaleSubDevice1, baseScaleSubDevice0, baseScaleSubDevice1, pwrBalance};
        for (const auto &node : nodeList) {
            if ((file.compare(node) == 0)) {
                val = -1.0;
                return ZE_RESULT_SUCCESS;
            }
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getValMediaBaseFailure(const std::string file, double &val) {
        if ((file.compare(baseFreqFactorSubDevice0) == 0) || (file.compare(baseFreqFactorSubDevice1) == 0) || (file.compare(mediaFreqFactorSubDevice0) == 0) || (file.compare(mediaFreqFactorSubDevice1) == 0) || (file.compare(pwrBalance) == 0)) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        if ((file.compare(mediaScaleSubDevice0) == 0) || (file.compare(mediaScaleSubDevice1) == 0) || (file.compare(baseScaleSubDevice0) == 0) || (file.compare(baseScaleSubDevice1) == 0)) {
            val = mockScale;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValComputeInvalid(const std::string file, double &val) {
        if ((file.compare(baseFreqFactorSubDevice0) == 0) || (file.compare(baseFreqFactorSubDevice1) == 0)) {
            val = 1024.0;
        } else if ((file.compare(mediaScaleSubDevice0) == 0) || (file.compare(mediaScaleSubDevice1) == 0) || (file.compare(baseScaleSubDevice0) == 0) || (file.compare(baseScaleSubDevice1) == 0)) {
            val = mockScale;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValScaleFailure(const std::string file, double &val) {
        if ((file.compare(mediaScaleSubDevice0) == 0) || (file.compare(mediaScaleSubDevice1) == 0) || (file.compare(baseScaleSubDevice0) == 0) || (file.compare(baseScaleSubDevice1) == 0)) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t canRead(const std::string file) override {
        if (mockCanReadResult != ZE_RESULT_SUCCESS) {
            return mockCanReadResult;
        }

        if ((file.compare(baseFreqFactorSubDevice0) == 0) || (file.compare(baseFreqFactorSubDevice1) == 0) || (file.compare(mediaFreqFactorSubDevice0) == 0) || (file.compare(mediaFreqFactorSubDevice1) == 0) || (file.compare(pwrBalance) == 0)) {
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    }

    ze_result_t write(const std::string file, const double val) override {
        if ((file.compare(baseFreqFactorSubDevice0) == 0) || (file.compare(baseFreqFactorSubDevice1) == 0)) {
            mockBaseVal1 = val;
        } else if ((file.compare(mediaFreqFactorSubDevice0) == 0) || (file.compare(mediaFreqFactorSubDevice1) == 0)) {
            mockMediaVal1 = val;
        } else if (file.compare(pwrBalance) == 0) {
            mockPwrBalance = val;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t read(const std::string file, double &val) override {
        if (mockReadResult != ZE_RESULT_SUCCESS) {
            return mockReadResult;
        }

        if (isReturnUnknownFailure) {
            return getValUnknownFailure(file, val);
        } else if (isMediaBaseFailure) {
            return getValMediaBaseFailure(file, val);
        } else if (isComputeInvalid) {
            return getValComputeInvalid(file, val);
        } else if (returnNegativeFactor) {
            return getValNegative(file, val);
        } else {
            return getValWithMultiplierReturnOne(file, val);
        }
    }

    double mockBaseVal1 = 128.0;
    double mockMediaVal1 = 256.0;
    double mockMultiplierBase = 256.0;
    double mockMultiplierMedia = 512.0;
    double mockScale = 0.00390625;
    double mockPwrBalance = 0.0;
    Mock<PerformanceSysfsAccess>() = default;
};

class PublicLinuxPerformanceImp : public L0::LinuxPerformanceImp {
  public:
    PublicLinuxPerformanceImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_engine_type_flag_t domain) : LinuxPerformanceImp(pOsSysman, onSubdevice, subdeviceId, domain) {}
    using LinuxPerformanceImp::domain;
    using LinuxPerformanceImp::pSysfsAccess;
};

} // namespace ult
} // namespace L0
