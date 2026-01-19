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
    std::string mockFdoValue = "enabled";

    ze_result_t read(const std::string file, std::string &val) override {

        if (readResult != ZE_RESULT_SUCCESS) {
            return readResult;
        }

        if (!file.compare("device/survivability_info/fdo_mode")) {
            val = mockFdoValue;
        }
        return ZE_RESULT_SUCCESS;
    }

    MockFdoSysFsAccessInterface() = default;
    ~MockFdoSysFsAccessInterface() override = default;
};

} // namespace ult
} // namespace Sysman
} // namespace L0