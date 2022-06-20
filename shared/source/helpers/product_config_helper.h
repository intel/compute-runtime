/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/const_stringref.h"

#include <sstream>
#include <string>

namespace AOT {
enum PRODUCT_CONFIG : uint32_t;
enum RELEASE : uint32_t;
enum FAMILY : uint32_t;
} // namespace AOT

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
    template <typename EqComparableT>
    static auto findAcronymWithoutDash(const EqComparableT &lhs) {
        return [&lhs](const auto &rhs) {
            return lhs == rhs || rhs.isEqualWithoutSeparator('-', lhs.c_str());
        };
    }

    template <typename EqComparableT>
    static auto findMapAcronymWithoutDash(const EqComparableT &lhs) {
        return [&lhs](const auto &rhs) {
            NEO::ConstStringRef ptrStr(rhs.first);
            return lhs == ptrStr || ptrStr.isEqualWithoutSeparator('-', lhs.c_str());
        };
    }
    static void adjustDeviceName(std::string &device);
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
