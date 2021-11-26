/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/directory.h"

#include <cstdio>
#include <dirent.h>

namespace NEO {

std::vector<std::string> Directory::getFiles(const std::string &path) {
    std::vector<std::string> files;
    if (path == "./test_files/linux/by-path") {
        files.push_back("./test_files/linux/by-path/pci-0000:00:02.0-card");
        files.push_back("./test_files/linux/by-path/pci-0000:00:02.0-render");
        files.push_back("./test_files/linux/by-path/pci-0000:00:03.0-card");
        files.push_back("./test_files/linux/by-path/pci-0000:00:03.0-render");
        return files;
    }
    if (path == "./test_files/linux/devices/device/drm") {
        files.push_back("./test_files/linux/devices/device/drm/card1");
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
    return files;
}
}; // namespace NEO
