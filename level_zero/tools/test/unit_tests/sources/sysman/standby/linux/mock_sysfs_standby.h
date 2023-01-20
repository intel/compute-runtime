/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/tools/source/sysman/standby/linux/os_standby_imp.h"

namespace L0 {
namespace ult {

const std::string standbyModeFile("gt/gt0/rc6_enable");
const std::string standbyModeFile1("gt/gt1/rc6_enable");
const std::string standbyModeFileLegacy("power/rc6_enable");

struct MockStandbySysfsAccess : public SysfsAccess {
    ze_result_t mockError = ZE_RESULT_SUCCESS;
    int mockStandbyMode = -1;
    bool isStandbyModeFileAvailable = true;
    ::mode_t mockStandbyFileMode = S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR;
    ADDMETHOD_NOBASE(directoryExists, bool, true, (const std::string path));

    ze_result_t read(const std::string file, int &val) override {
        return getVal(file, val);
    }

    ze_result_t write(const std::string file, int val) override {
        return setVal(file, val);
    }

    ze_result_t canRead(const std::string file) override {
        return getCanReadStatus(file);
    }

    ze_result_t getCanReadStatus(const std::string file) {
        if (isFileAccessible(file) == true) {
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t getVal(const std::string file, int &val) {
        if (mockError != ZE_RESULT_SUCCESS) {
            return mockError;
        }
        if ((isFileAccessible(file) == true) &&
            (mockStandbyFileMode & S_IRUSR) != 0) {
            val = mockStandbyMode;
            return ZE_RESULT_SUCCESS;
        }

        if (isStandbyModeFileAvailable == false) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        if ((mockStandbyFileMode & S_IRUSR) == 0) {
            return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
        }

        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t setVal(const std::string file, const int val) {
        if ((isFileAccessible(file) == true) &&
            (mockStandbyFileMode & S_IWUSR) != 0) {
            mockStandbyMode = val;
            return ZE_RESULT_SUCCESS;
        }

        if (isFileAccessible(file) == false) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        if ((mockStandbyFileMode & S_IWUSR) == 0) {
            return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
        }

        return ZE_RESULT_ERROR_UNKNOWN;
    }

    void setValReturnError(ze_result_t error) {
        mockError = error;
    }

    MockStandbySysfsAccess() = default;
    ~MockStandbySysfsAccess() override = default;

  private:
    bool isFileAccessible(const std::string file) {
        if (((file.compare(standbyModeFile) == 0) || (file.compare(standbyModeFile1) == 0) || (file.compare(standbyModeFileLegacy) == 0)) && (isStandbyModeFileAvailable == true)) {
            return true;
        }
        return false;
    }
};

class PublicLinuxStandbyImp : public L0::LinuxStandbyImp {
  public:
    PublicLinuxStandbyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : LinuxStandbyImp(pOsSysman, onSubdevice, subdeviceId) {}
    using LinuxStandbyImp::pSysfsAccess;
};
} // namespace ult
} // namespace L0
