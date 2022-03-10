/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "platforms.h"

#include <sstream>
#include <string>

namespace NEO {
struct ProductConfigHelper {
    static uint32_t getMajor(PRODUCT_CONFIG config) {
        return (static_cast<uint32_t>(config) & 0xff0000) >> 16;
    }

    static uint32_t getMinor(PRODUCT_CONFIG config) {
        return (static_cast<uint32_t>(config) & 0x00ff00) >> 8;
    }

    static uint32_t getRevision(PRODUCT_CONFIG config) {
        return static_cast<uint32_t>(config) & 0x0000ff;
    }

    static std::string parseMajorMinorRevisionValue(PRODUCT_CONFIG config) {
        std::stringstream stringConfig;
        stringConfig << getMajor(config) << "." << getMinor(config) << "." << getRevision(config);
        return stringConfig.str();
    }

    static std::string parseMajorMinorValue(PRODUCT_CONFIG config) {
        std::stringstream stringConfig;
        stringConfig << getMajor(config) << "." << getMinor(config);
        return stringConfig.str();
    }
};

} // namespace NEO
