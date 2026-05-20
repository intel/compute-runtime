/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/test/unit_tests/sources/vf_management/linux/mock_sysfs_vf_management.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr std::string_view fileForXeLmemQuota = "device/sriov_admin/vf1/profile/vram_quota";

struct MockVfXeSysfsAccessInterface : public MockVfSysfsAccessInterface {

    ze_result_t read(const std::string file, uint64_t &val) override {
        if (mockReadMemoryError != ZE_RESULT_SUCCESS) {
            return mockReadMemoryError;
        }
        if (file.compare(fileForXeLmemQuota) == 0) {
            val = mockLmemQuotaValue ? mockLmemQuota : 0;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
