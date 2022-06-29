/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/sysman.h"

#include "sysman/linux/os_sysman_imp.h"

using ::testing::_;
using ::testing::NiceMock;
using namespace NEO;

namespace L0 {
namespace ult {
const std::string mockedDeviceName("/MOCK_DEVICE_NAME");

class LinuxProcfsAccess : public ProcfsAccess {};

template <>
struct Mock<LinuxProcfsAccess> : public LinuxProcfsAccess {
    ::pid_t ourDevicePid = 0;
    int ourDeviceFd = 0;
    ze_result_t getMockFileName(const ::pid_t pid, const int fd, std::string &val) {
        if (pid == ourDevicePid && fd == ourDeviceFd) {
            val = mockedDeviceName;
        } else {
            // return fake filenames for other file descriptors
            val = std::string("/FILENAME") + std::to_string(fd);
        }
        return ZE_RESULT_SUCCESS;
    }

    Mock<LinuxProcfsAccess>() = default;

    MOCK_METHOD(ze_result_t, getFileName, (const ::pid_t pid, const int fd, std::string &val), (override));
};

} // namespace ult
} // namespace L0
