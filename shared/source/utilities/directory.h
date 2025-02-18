/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <cinttypes>
#include <optional>
#include <string.h>
#include <string>
#include <vector>

namespace NEO {

class Directory : NEO::NonCopyableAndNonMovableClass {
  public:
    static inline constexpr char returnDirs{1 << 0};
    static inline constexpr char createDirs{1 << 1};

    static std::vector<std::string> getFiles(const std::string &path);
    static void createDirectory(const std::string &path);

    Directory() = default;
    Directory(const std::string &path) : path(path) {}

    inline std::optional<std::vector<std::string>> parseDirectories(char flags) {
        std::optional<std::vector<std::string>> directories;
        if (flags & returnDirs) {
            directories.emplace();
        }
        std::string tmp;
        size_t pos = 0;

        while (pos != std::string::npos) {
            pos = this->path.find_first_of("/\\", pos + 1);
            tmp = this->path.substr(0, pos);
            if (flags & createDirs) {
                this->create(tmp);
            }
            if (flags & returnDirs) {
                directories->push_back(tmp);
            }
        }
        return directories;
    }

    void operator()(const std::string &path) {
        this->path = path;
    }

  protected:
    virtual void create(const std::string &path) {
        createDirectory(path);
    }

    std::string path;
};

static_assert(NEO::NonCopyableAndNonMovable<Directory>);

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
