/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/events/events_imp.h"
#include "level_zero/tools/source/sysman/events/linux/os_events_imp.h"

namespace L0 {
namespace ult {

const std::string ueventWedgedFile("/var/lib/libze_intel_gpu/wedged_file");

class EventsFsAccess : public FsAccess {};

template <>
struct Mock<EventsFsAccess> : public EventsFsAccess {

    ze_result_t getValWedgedFileTrue(const std::string file, uint32_t &val) {
        if (file.compare(ueventWedgedFile) == 0) {
            val = 1;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValWedgedFileFalse(const std::string file, uint32_t &val) {
        if (file.compare(ueventWedgedFile) == 0) {
            val = 0;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValWedgedFileNotFound(const std::string file, uint32_t &val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getValWedgedFileInsufficientPermissions(const std::string file, uint32_t &val) {
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    }

    Mock<EventsFsAccess>() = default;

    MOCK_METHOD(ze_result_t, read, (const std::string file, std::string &val), (override));
    MOCK_METHOD(ze_result_t, read, (const std::string file, uint32_t &val), (override));
    MOCK_METHOD(ze_result_t, canWrite, (const std::string file), (override));
};

} // namespace ult
} // namespace L0
