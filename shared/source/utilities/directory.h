/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cinttypes>
#include <optional>
#include <string.h>
#include <string>
#include <vector>

namespace NEO {

namespace Directory {
extern bool returnEmptyFilesVector;
inline constexpr char returnDirs = 1 << 0;
inline constexpr char createDirs = 1 << 1;

std::vector<std::string> getFiles(const std::string &path);
void createDirectory(const std::string &path);

inline std::optional<std::vector<std::string>> getDirectories(const std::string &path, char flags) {
    std::optional<std::vector<std::string>> directories;
    if (flags & returnDirs) {
        directories.emplace();
    }
    std::string tmp;
    size_t pos = 0;

    while (pos != std::string::npos) {
        pos = path.find_first_of("/\\", pos + 1);
        tmp = path.substr(0, pos);
        if (flags & createDirs) {
            createDirectory(tmp);
        }
        if (flags & returnDirs) {
            directories->push_back(tmp);
        }
    }
    return directories;
}

} // namespace Directory

inline int parseBdfString(const std::string &pciBDF, uint16_t &domain, uint8_t &bus, uint8_t &device, uint8_t &function) {
    if (strlen(pciBDF.c_str()) == 12) {
        domain = static_cast<uint16_t>(strtol((pciBDF.substr(0, 4)).c_str(), NULL, 16));
        bus = static_cast<uint8_t>(strtol((pciBDF.substr(5, 2)).c_str(), NULL, 16));
        device = static_cast<uint8_t>(strtol((pciBDF.substr(8, 2)).c_str(), NULL, 16));
        function = static_cast<uint8_t>(strtol((pciBDF.substr(11, 1)).c_str(), NULL, 16));
        return 4;
    } else {
        return 0;
    }
}

} // namespace NEO
