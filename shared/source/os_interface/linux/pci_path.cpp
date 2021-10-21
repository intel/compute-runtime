/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/pci_path.h"

#include "shared/source/os_interface/linux/sys_calls.h"

#include <string_view>
#include <unistd.h>

namespace NEO {
std::optional<std::string> getPciPath(int deviceFd) {
    char path[256] = {0};
    size_t pathlen = 256;

    if (SysCalls::getDevicePath(deviceFd, path, pathlen)) {
        return std::nullopt;
    }

    if (SysCalls::access(path, F_OK)) {
        return std::nullopt;
    }

    int readLinkSize = 0;
    char devicePath[256] = {0};
    readLinkSize = SysCalls::readlink(path, devicePath, pathlen);

    if (readLinkSize == -1) {
        return std::nullopt;
    }

    std::string_view devicePathView(devicePath, static_cast<size_t>(readLinkSize));
    devicePathView = devicePathView.substr(devicePathView.find("/drm/render") - 12u, 12u);

    return std::string(devicePathView);
}
} // namespace NEO
