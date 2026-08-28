/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

#include <string_view>

namespace L0 {
namespace Sysman {
namespace ult {

struct MockFsAccessInterface : public L0::Sysman::FsAccessInterface {
    ze_result_t readResult = ZE_RESULT_SUCCESS;
    std::string mockFdoValue = "enabled";
    ze_result_t readSymLinkResult = ZE_RESULT_SUCCESS;
    std::string mockDriverSymLinkValue = "../../../../../../bus/pci/drivers/xe";
    std::string readSymLinkPathRequested;

    ze_result_t read(const std::string file, std::string &val) override {
        if (readResult != ZE_RESULT_SUCCESS) {
            return readResult;
        }

        if (file.find("/survivability_info/fdo_mode") != std::string::npos) {
            val = mockFdoValue;
        }

        return ZE_RESULT_SUCCESS;
    }

    // The driver symlink is looked up by absolute path, so only an absolute
    // path under /sys/bus/pci/devices resolves here. Anything else means the
    // caller went through the per-device sysfs interface, which would prepend
    // its own directory and never find the entry on a real system.
    ze_result_t readSymLink(const std::string path, std::string &buf) override {
        readSymLinkPathRequested = path;
        if (readSymLinkResult != ZE_RESULT_SUCCESS) {
            return readSymLinkResult;
        }

        const std::string_view expectedPrefix = "/sys/bus/pci/devices/";
        if (std::string_view(path).substr(0, expectedPrefix.size()) != expectedPrefix) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        if (path.find("/driver") != std::string::npos) {
            buf = mockDriverSymLinkValue;
        }

        return ZE_RESULT_SUCCESS;
    }

    MockFsAccessInterface() = default;
    ~MockFsAccessInterface() override = default;
};

struct MockSysFsAccessInterface : public L0::Sysman::SysFsAccessInterface {
    ze_result_t realPathResult = ZE_RESULT_SUCCESS;
    std::string mockRealPathValue = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0";

    ze_result_t getRealPath(const std::string &path, std::string &val) override {
        if (realPathResult != ZE_RESULT_SUCCESS) {
            return realPathResult;
        }
        if (path == "device" || path == "device/") {
            val = mockRealPathValue;
            return ZE_RESULT_SUCCESS;
        }
        if (path == "device/config") {
            val = mockRealPathValue + "/config";
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    MockSysFsAccessInterface() = default;
    ~MockSysFsAccessInterface() override = default;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
