/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/standby/linux/os_standby_imp.h"

namespace L0 {
namespace ult {

const std::string standbyModeFile("power/rc6_enable");

class StandbySysfsAccess : public SysfsAccess {};

template <>
struct Mock<StandbySysfsAccess> : public StandbySysfsAccess {
    int mockStandbyMode = -1;
    MOCK_METHOD(ze_result_t, read, (const std::string file, int &val), (override));
    MOCK_METHOD(ze_result_t, canRead, (const std::string file), (override));

    ze_result_t getCanReadStatus(const std::string file) {
        if (file.compare(standbyModeFile) == 0) {
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    ze_result_t getVal(const std::string file, int &val) {
        if (file.compare(standbyModeFile) == 0) {
            val = mockStandbyMode;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t setVal(const std::string file, const int val) {
        if (file.compare(standbyModeFile) == 0) {
            mockStandbyMode = val;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t setValReturnError(const std::string file, const int val) {
        if (file.compare(standbyModeFile) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    Mock() = default;
    ~Mock() override = default;
};

class PublicLinuxStandbyImp : public L0::LinuxStandbyImp {
  public:
    using LinuxStandbyImp::pSysfsAccess;
};
} // namespace ult
} // namespace L0
