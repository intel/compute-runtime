/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/sysman.h"

using namespace NEO;

namespace L0 {
namespace ult {

struct MockLinuxSysfsAccess : public SysfsAccess {

    ze_result_t getRealPath(const std::string file, std::string &val) override {
        val = "/random/path";
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t read(const std::string file, std::string &val) override {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, int32_t &val) override {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, uint32_t &val) override {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, uint64_t &val) override {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, double &val) override {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, std::vector<std::string> &val) override {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t readSymLink(const std::string path, std::string &buf) override {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    MockLinuxSysfsAccess() = default;
};

} // namespace ult
} // namespace L0
