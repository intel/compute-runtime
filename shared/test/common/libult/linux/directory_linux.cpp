/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/directory.h"

#include "test_files_setup.h"

#include <cstdio>
#include <dirent.h>
#include <map>

namespace NEO {

std::string byPathPattern(std::string(NEO_SHARED_TEST_FILES_DIR) + "/linux/by-path");
std::string deviceDrmPath(std::string(NEO_SHARED_TEST_FILES_DIR) + "/linux/devices/device/drm");
std::map<std::string, std::vector<std::string>> directoryFilesMap = {};

std::vector<std::string> Directory::getFiles(const std::string &path) {
    std::vector<std::string> files;
    if (path == byPathPattern) {
        files.push_back(byPathPattern + "/pci-0000:00:02.0-card");
        files.push_back(byPathPattern + "/pci-0000:00:02.0-render");
        files.push_back(byPathPattern + "/pci-0000:00:03.0-card");
        files.push_back(byPathPattern + "/pci-0000:00:03.0-render");
        return files;
    }
    if (path == deviceDrmPath) {
        files.push_back(deviceDrmPath + "/card1");
        return files;
    }
    if (path == "/sys/class/intel_pmt") {
        return {
            "/sys/class/intel_pmt/crashlog1",
            "/sys/class/intel_pmt/crashlog2",
            "/sys/class/intel_pmt/crashlog3",
            "/sys/class/intel_pmt/crashlog4",
            "/sys/class/intel_pmt/telem2",
            "/sys/class/intel_pmt/telem1",
            "/sys/class/intel_pmt/telem10",
            "/sys/class/intel_pmt/telem11",
            "/sys/class/intel_pmt/telem12",
            "/sys/class/intel_pmt/telem3",
            "/sys/class/intel_pmt/telem5",
            "/sys/class/intel_pmt/telem4",
            "/sys/class/intel_pmt/telem6",
            "/sys/class/intel_pmt/telem8",
            "/sys/class/intel_pmt/telem7",
            "/sys/class/intel_pmt/telem9",
        };
    }

    auto it = directoryFilesMap.find(path);
    if (it != directoryFilesMap.end()) {
        return directoryFilesMap[path];
    }

    return files;
}
}; // namespace NEO
