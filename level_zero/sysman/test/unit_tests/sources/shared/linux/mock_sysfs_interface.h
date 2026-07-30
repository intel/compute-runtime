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

struct MockFdoFsAccessInterface : public L0::Sysman::FsAccessInterface {
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

    MockFdoFsAccessInterface() = default;
    ~MockFdoFsAccessInterface() override = default;
};

struct MockFdoSysFsAccessInterface : public L0::Sysman::SysFsAccessInterface {
    MockFdoSysFsAccessInterface() = default;
    ~MockFdoSysFsAccessInterface() override = default;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
