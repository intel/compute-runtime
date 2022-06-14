/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"

#include <algorithm>

void ProductConfigHelper::adjustDeviceName(std::string &device) {
    std::transform(device.begin(), device.end(), device.begin(), ::tolower);

    auto findCore = device.find("_core");
    if (findCore != std::string::npos) {
        device = device.substr(0, findCore);
    }

    auto findUnderscore = device.find("_");
    if (findUnderscore != std::string::npos) {
        device.erase(std::remove(device.begin(), device.end(), '_'), device.end());
    }
}

AOT::RELEASE ProductConfigHelper::returnReleaseForAcronym(const std::string &device) {
    auto it = std::find_if(AOT::releaseAcronyms.begin(), AOT::releaseAcronyms.end(), findMapAcronymWithoutDash(device));
    if (it == AOT::releaseAcronyms.end())
        return AOT::UNKNOWN_RELEASE;
    return it->second;
}

AOT::FAMILY ProductConfigHelper::returnFamilyForAcronym(const std::string &device) {
    auto it = std::find_if(AOT::familyAcronyms.begin(), AOT::familyAcronyms.end(), findMapAcronymWithoutDash(device));
    if (it == AOT::familyAcronyms.end())
        return AOT::UNKNOWN_FAMILY;
    return it->second;
}

AOT::PRODUCT_CONFIG ProductConfigHelper::returnProductConfigForAcronym(const std::string &device) {
    auto it = std::find_if(AOT::productConfigAcronyms.begin(), AOT::productConfigAcronyms.end(), findMapAcronymWithoutDash(device));
    if (it == AOT::productConfigAcronyms.end())
        return AOT::UNKNOWN_ISA;
    return it->second;
}

NEO::ConstStringRef ProductConfigHelper::getAcronymForAFamily(AOT::FAMILY family) {
    for (const auto &[acronym, value] : AOT::familyAcronyms) {
        if (value == family) {
            return NEO::ConstStringRef(acronym);
        }
    }
    return {};
}

std::string ProductConfigHelper::parseMajorMinorRevisionValue(AheadOfTimeConfig config) {
    std::stringstream stringConfig;
    stringConfig << config.ProductConfigID.Major << "." << config.ProductConfigID.Minor << "." << config.ProductConfigID.Revision;
    return stringConfig.str();
}

std::string ProductConfigHelper::parseMajorMinorValue(AheadOfTimeConfig config) {
    std::stringstream stringConfig;
    stringConfig << config.ProductConfigID.Major << "." << config.ProductConfigID.Minor;
    return stringConfig.str();
}
