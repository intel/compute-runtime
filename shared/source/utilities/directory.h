/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cinttypes>
#include <string.h>
#include <string>
#include <vector>

namespace NEO {

class Directory {
  public:
    static std::vector<std::string> getFiles(const std::string &path);
};

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
