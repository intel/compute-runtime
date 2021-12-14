/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/sysman.h"

#include "sysman/linux/os_sysman_imp.h"

using ::testing::_;
using ::testing::NiceMock;
using namespace NEO;

namespace L0 {
namespace ult {

class LinuxSysfsAccess : public SysfsAccess {};
template <>
struct Mock<LinuxSysfsAccess> : public LinuxSysfsAccess {

    ze_result_t getRealPathVal(const std::string file, std::string &val) {
        val = "/random/path";
        return ZE_RESULT_SUCCESS;
    }

    Mock<LinuxSysfsAccess>() = default;

    MOCK_METHOD(ze_result_t, getRealPath, (const std::string path, std::string &val), (override));
};

} // namespace ult
} // namespace L0
