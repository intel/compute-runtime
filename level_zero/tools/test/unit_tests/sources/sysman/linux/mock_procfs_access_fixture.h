/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/sysman.h"

using namespace NEO;

namespace L0 {
namespace ult {
const std::string mockedDeviceName("/MOCK_DEVICE_NAME");

struct MockLinuxProcfsAccess : public ProcfsAccess {
    ::pid_t ourDevicePid = 0;
    int ourDeviceFd = 0;
    ze_result_t getFileName(const ::pid_t pid, const int fd, std::string &val) override {
        if (pid == ourDevicePid && fd == ourDeviceFd) {
            val = mockedDeviceName;
        } else {
            // return fake filenames for other file descriptors
            val = std::string("/FILENAME") + std::to_string(fd);
        }
        return ZE_RESULT_SUCCESS;
    }

    MockLinuxProcfsAccess() = default;
};

} // namespace ult
} // namespace L0
