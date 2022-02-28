/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/pci_path.h"

#include "shared/source/os_interface/linux/sys_calls.h"

#include <string_view>
#include <unistd.h>

namespace NEO {
std::optional<std::string> getPciLinkPath(int deviceFd) {
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
    size_t devicePathLength = 256;
    readLinkSize = SysCalls::readlink(path, devicePath, devicePathLength);

    if (readLinkSize == -1) {
        return std::nullopt;
    }

    return std::string(devicePath, static_cast<size_t>(readLinkSize));
}

std::optional<std::string> getPciPath(int deviceFd) {

    auto deviceLinkPath = NEO::getPciLinkPath(deviceFd);

    if (deviceLinkPath == std::nullopt) {
        return std::nullopt;
    }

    return deviceLinkPath->substr(deviceLinkPath->find("/drm/render") - 12u, 12u);
}

std::optional<std::string> getPciRootPath(int deviceFd) {

    auto pciLinkPath = NEO::getPciLinkPath(deviceFd);
    // pciLinkPath = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128/",
    // Then root path = "/pci0000:37/0000:37:01.0/0000:38:00.0/"
    if (pciLinkPath == std::nullopt) {
        return std::nullopt;
    }

    auto startPos = pciLinkPath->find("/pci");
    if (startPos == std::string::npos) {
        return std::nullopt;
    }

    // Root PCI path is at 2 levels up from drm/renderD128
    uint32_t rootPciDepth = 4;
    auto endPos = std::string::npos;
    while (rootPciDepth--) {
        endPos = pciLinkPath->rfind('/', endPos - 1);
        if (endPos == std::string::npos) {
            return std::nullopt;
        }
    }

    return pciLinkPath->substr(startPos, endPos - startPos);
}

} // namespace NEO
