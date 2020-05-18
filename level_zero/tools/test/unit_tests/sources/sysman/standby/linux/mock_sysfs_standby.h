/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/standby/linux/os_standby_imp.h"

#include "sysman/linux/fs_access.h"
#include "sysman/standby/os_standby.h"
#include "sysman/standby/standby_imp.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

using ::testing::_;

namespace L0 {
namespace ult {

const std::string standbyModeFile("power/rc6_enable");

class StandbySysfsAccess : public SysfsAccess {};

template <>
struct Mock<StandbySysfsAccess> : public StandbySysfsAccess {
    int mockStandbyMode = -1;
    MOCK_METHOD2(read, ze_result_t(const std::string file, int &val));
    MOCK_METHOD2(write, ze_result_t(const std::string file, const int val));

    ze_result_t getVal(const std::string file, int &val) {
        if (file.compare(standbyModeFile) == 0) {
            val = mockStandbyMode;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setVal(const std::string file, const int val) {
        if (file.compare(standbyModeFile) == 0) {
            mockStandbyMode = val;
        }
        return ZE_RESULT_SUCCESS;
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

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
