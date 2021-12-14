/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/pmt_util.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/utilities/directory.h"

#include <climits>

#include <algorithm>
#include <array>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <sstream>
#include <string_view>

namespace NEO {

void PmtUtil::getTelemNodesInPciPath(std::string_view rootPciPath, std::map<uint32_t, std::string> &telemPciPath) {

    // if bdf = 0000:3a:00.0
    // link = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128/",
    // then root path = "/pci0000:37/0000:37:01.0"
    auto telemNodeList = Directory::getFiles("/sys/class/intel_pmt");
    std::string_view telemDirString("/sys/class/intel_pmt/telem");
    telemPciPath.clear();

    // Get the telem nodes under the same root pci path
    for (auto &telemNode : telemNodeList) {
        char symlink[256] = {'\0'};
        std::size_t pos = telemNode.find(telemDirString);
        if (pos != std::string::npos) {
            pos += telemDirString.length();
            int len = SysCalls::readlink(telemNode.c_str(), symlink, sizeof(symlink) - 1);
            if (len != -1) {
                symlink[len] = '\0';
                std::string_view symlinkString(symlink);
                if (symlinkString.find(rootPciPath.data()) != std::string::npos) {
                    int index = std::stoi(telemNode.substr(pos));
                    telemPciPath[index] = telemNode;
                }
            }
        }
    }
}

bool PmtUtil::readGuid(std::string_view telemDir, std::array<char, PmtUtil::guidStringSize> &guidString) {
    std::ostringstream guidFilename;
    guidFilename << telemDir << "/guid";
    int fd = SysCalls::open(guidFilename.str().c_str(), O_RDONLY);
    if (fd <= 0) {
        return false;
    }
    guidString.fill('\0');
    ssize_t bytesRead = SysCalls::pread(fd, guidString.data(), guidString.size() - 1, 0);
    SysCalls::close(fd);
    if (bytesRead <= 0) {
        return false;
    }
    std::replace(guidString.begin(), guidString.end(), '\n', '\0');
    return true;
}

bool PmtUtil::readOffset(std::string_view telemDir, uint64_t &offset) {

    std::ostringstream offsetFilename;
    offsetFilename << telemDir << "/offset";
    int fd = SysCalls::open(offsetFilename.str().c_str(), O_RDONLY);
    if (fd <= 0) {
        return false;
    }
    offset = ULONG_MAX;
    std::array<char, 16> offsetString = {'\0'};
    ssize_t bytesRead = SysCalls::pread(fd, offsetString.data(), offsetString.size() - 1, 0);
    if (bytesRead > 0) {
        std::replace(offsetString.begin(), offsetString.end(), '\n', '\0');
        offset = std::strtoul(offsetString.data(), nullptr, 10);
    }
    SysCalls::close(fd);

    if (offset == ULONG_MAX) {
        return false;
    }
    return true;
}

ssize_t PmtUtil::readTelem(std::string_view telemDir, const std::size_t count, const uint64_t offset, void *data) {

    if (data == nullptr) {
        return 0;
    }

    ssize_t bytesRead = 0;
    std::ostringstream telemFilename;
    telemFilename << telemDir << "/telem";
    int fd = SysCalls::open(telemFilename.str().c_str(), O_RDONLY);
    if (fd > 0) {
        bytesRead = SysCalls::pread(fd, data, count, static_cast<off_t>(offset));
        SysCalls::close(fd);
    }
    return bytesRead;
}

} // namespace NEO