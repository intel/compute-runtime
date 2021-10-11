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
    return files;
}
}; // namespace NEO
