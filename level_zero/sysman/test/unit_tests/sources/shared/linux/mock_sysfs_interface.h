/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct MockFdoSysFsAccessInterface : public L0::Sysman::SysFsAccessInterface {
    ze_result_t readResult = ZE_RESULT_SUCCESS;
    std::vector<std::string> mockFileContent;

    ze_result_t read(const std::string file, std::vector<std::string> &val) override {
        if (readResult != ZE_RESULT_SUCCESS) {
            return readResult;
        }

        if (file == "/survivability_mode") {
            val = mockFileContent;
            return ZE_RESULT_SUCCESS;
        }

        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    MockFdoSysFsAccessInterface() = default;
    ~MockFdoSysFsAccessInterface() override = default;
};

} // namespace ult
} // namespace Sysman
} // namespace L0