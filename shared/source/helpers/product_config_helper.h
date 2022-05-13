/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/const_stringref.h"

#include "platforms.h"

#include <sstream>
#include <string>

struct AheadOfTimeConfig {
    union {
        uint32_t ProductConfig;
        struct
        {
            uint32_t Revision : 6;
            uint32_t Reserved : 8;
            uint32_t Minor : 8;
            uint32_t Major : 10;
        } ProductConfigID;
    };
};

struct ProductConfigHelper {
    static std::string parseMajorMinorValue(AheadOfTimeConfig config);
    static std::string parseMajorMinorRevisionValue(AheadOfTimeConfig config);
    inline static std::string parseMajorMinorRevisionValue(AOT::PRODUCT_CONFIG config) {
        std::stringstream stringConfig;
        AheadOfTimeConfig aotConfig = {0};
        aotConfig.ProductConfig = config;
        return parseMajorMinorRevisionValue(aotConfig);
    }

    static AOT::PRODUCT_CONFIG returnProductConfigForAcronym(const std::string &device);
    static AOT::RELEASE returnReleaseForAcronym(const std::string &device);
    static AOT::FAMILY returnFamilyForAcronym(const std::string &device);
    static NEO::ConstStringRef getAcronymForAFamily(AOT::FAMILY family);
};
