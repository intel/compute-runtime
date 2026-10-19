/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/linux/fs_access.h"

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
