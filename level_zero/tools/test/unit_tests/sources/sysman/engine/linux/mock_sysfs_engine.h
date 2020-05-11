/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"

#include "sysman/engine/engine_imp.h"
#include "sysman/engine/os_engine.h"
#include "sysman/linux/fs_access.h"
#include "sysman/sysman_imp.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

using ::testing::_;

namespace L0 {
namespace ult {

const std::string computeEngineGroupFile("engine/rcs0/name");

class EngineSysfsAccess : public SysfsAccess {};

template <>
struct Mock<EngineSysfsAccess> : public EngineSysfsAccess {
    MOCK_METHOD2(read, ze_result_t(const std::string file, std::string &val));

    ze_result_t getVal(const std::string file, std::string &val) {
        if (file.compare(computeEngineGroupFile) == 0) {
            val = "rcs0";
        }
        return ZE_RESULT_SUCCESS;
    }

    Mock<EngineSysfsAccess>() = default;
};
class PublicLinuxEngineImp : public L0::LinuxEngineImp {
  public:
    using LinuxEngineImp::pSysfsAccess;
};
} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
