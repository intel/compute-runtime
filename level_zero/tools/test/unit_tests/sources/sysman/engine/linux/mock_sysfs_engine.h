/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"

#include "sysman/engine/engine_imp.h"
#include "sysman/linux/fs_access.h"

using ::testing::_;

namespace L0 {
namespace ult {

const std::string computeEngineGroupFile("engine/rcs0/name");

class EngineSysfsAccess : public SysfsAccess {};

template <>
struct Mock<EngineSysfsAccess> : public EngineSysfsAccess {
    MOCK_METHOD(ze_result_t, read, (const std::string file, std::string &val), (override));

    ze_result_t getVal(const std::string file, std::string &val) {
        if (file.compare(computeEngineGroupFile) == 0) {
            val = "rcs0";
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t getValReturnError(const std::string file, std::string &val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    ze_result_t getIncorrectVal(const std::string file, std::string &val) {
        if (file.compare(computeEngineGroupFile) == 0) {
            val = "";
        }
        return ZE_RESULT_SUCCESS;
    }

    Mock() = default;
    ~Mock() override = default;
};
class PublicLinuxEngineImp : public L0::LinuxEngineImp {
  public:
    using LinuxEngineImp::pSysfsAccess;
};
} // namespace ult
} // namespace L0
