/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper_former.h"

#include "shared/source/helpers/hw_info.h"

#include "device_ids_configs.h"
#include "hw_cmds.h"
#include "neo_aot_platforms.h"

FormerProductConfigHelper::FormerProductConfigHelper()
    : helper(Ocloc::SupportedDevicesMode::concat, nullptr) {
    data = helper.collectFormerSupportedDevicesData();
}

Ocloc::SupportedDevicesHelper::SupportedDevicesData &FormerProductConfigHelper::getData() {
    return data;
}

template <>
bool FormerProductConfigHelper::isSupportedTarget<AOT::FAMILY>(const std::string &target) const {
    return std::any_of(
        data.familyGroups.begin(), data.familyGroups.end(),
        [&](const auto &pair) { return pair.first == target; });
}

template <>
bool FormerProductConfigHelper::isSupportedTarget<AOT::RELEASE>(const std::string &target) const {
    return std::any_of(
        data.releaseGroups.begin(), data.releaseGroups.end(),
        [&](const auto &pair) { return pair.first == target; });
}

bool FormerProductConfigHelper::isSupportedProductConfig(uint32_t config) const {
    if (config == AOT::UNKNOWN_ISA) {
        return false;
    }
    return std::any_of(data.deviceIpVersions.begin(), data.deviceIpVersions.end(),
                       [config](uint32_t v) { return v == config; });
}

std::vector<NEO::ConstStringRef> FormerProductConfigHelper::getProductAcronymsFromFamilyGroup(const std::string &groupName) const {
    return getProductAcronymsFromGroup(groupName, data.familyGroups);
}

std::vector<NEO::ConstStringRef> FormerProductConfigHelper::getProductAcronymsFromReleaseGroup(const std::string &groupName) const {
    return getProductAcronymsFromGroup(groupName, data.releaseGroups);
}

AOT::PRODUCT_CONFIG FormerProductConfigHelper::getProductConfigFromDeviceName(const std::string &device) {
    uint32_t config = AOT::UNKNOWN_ISA;
    char hexPrefixLength = 2;
    if (device.find(".") != std::string::npos) {
        config = ProductConfigHelper::getProductConfigFromVersionValue(device);
    } else if (std::all_of(device.begin(), device.end(), (::isdigit))) {
        config = static_cast<uint32_t>(std::stoul(device));
    } else if (device.substr(0, hexPrefixLength) == "0x" && std::all_of(device.begin() + hexPrefixLength, device.end(), (::isxdigit))) {
        auto deviceId = static_cast<unsigned short>(std::stoi(device, 0, 16));
        config = getProductConfigFromDeviceId(deviceId);
    } else {
        config = getProductConfigFromAcronym(device);
    }
    return static_cast<AOT::PRODUCT_CONFIG>(config);
}

AOT::PRODUCT_CONFIG FormerProductConfigHelper::getProductConfigFromAcronym(const std::string &device) {
    auto it = std::find_if(
        data.acronyms.begin(), data.acronyms.end(),
        [&](const auto &pair) { return pair.first == device; });
    if (it != data.acronyms.end()) {
        return static_cast<AOT::PRODUCT_CONFIG>(it->second);
    }
    return AOT::UNKNOWN_ISA;
}

AOT::PRODUCT_CONFIG FormerProductConfigHelper::getProductConfigFromDeviceId(const uint32_t &deviceId) {
    auto it = std::find_if(
        data.deviceInfos.begin(), data.deviceInfos.end(),
        [&](const auto &deviceInfo) { return deviceInfo.deviceId == deviceId; });
    if (it != data.deviceInfos.end()) {
        return static_cast<AOT::PRODUCT_CONFIG>(it->ipVersion);
    }
    return AOT::UNKNOWN_ISA;
}

AOT::PRODUCT_CONFIG FormerProductConfigHelper::getFirstProductConfig() {
    uint32_t firstProduct = AOT::UNKNOWN_ISA;
    if (!data.acronyms.empty()) {
        auto &firstProductPair = data.acronyms.front();
        firstProduct = firstProductPair.second;
    }
    return static_cast<AOT::PRODUCT_CONFIG>(firstProduct);
}

bool FormerProductConfigHelper::isFamilyName(const std::string &family) {
    return std::any_of(
        data.familyGroups.begin(), data.familyGroups.end(),
        [&family](const auto &pair) {
            return pair.first == family;
        });
}

bool FormerProductConfigHelper::isReleaseName(const std::string &release) {
    return std::any_of(
        data.releaseGroups.begin(), data.releaseGroups.end(),
        [&release](const auto &pair) {
            return pair.first == release;
        });
}

std::string FormerProductConfigHelper::getFamilyName(Position pos) {
    if (data.familyGroups.empty()) {
        return "";
    }
    if (pos == Position::firstItem) {
        return data.familyGroups.front().first;
    } else {
        return data.familyGroups.back().first;
    }
}

std::string FormerProductConfigHelper::getReleaseName(Position pos) {
    if (data.releaseGroups.empty()) {
        return "";
    }
    if (pos == Position::firstItem) {
        return data.releaseGroups.front().first;
    } else {
        return data.releaseGroups.back().first;
    }
}

AOT::PRODUCT_CONFIG FormerProductConfigHelper::getProductConfigFromFamilyName(const std::string &family, Position pos) {
    for (const auto &[acronym, value] : data.familyGroups) {
        if (acronym == family && !value.empty()) {
            if (pos == Position::firstItem) {
                return static_cast<AOT::PRODUCT_CONFIG>(value.front());
            } else {
                return static_cast<AOT::PRODUCT_CONFIG>(value.back());
            }
        }
    }
    return AOT::UNKNOWN_ISA;
}

AOT::PRODUCT_CONFIG FormerProductConfigHelper::getProductConfigFromReleaseName(const std::string &release, Position pos) {
    for (const auto &[acronym, value] : data.releaseGroups) {
        if (acronym == release && !value.empty()) {
            if (pos == Position::firstItem) {
                return static_cast<AOT::PRODUCT_CONFIG>(value.front());
            } else {
                return static_cast<AOT::PRODUCT_CONFIG>(value.back());
            }
        }
    }
    return AOT::UNKNOWN_ISA;
}

std::vector<NEO::ConstStringRef> FormerProductConfigHelper::getRepresentativeProductAcronyms() {
    std::vector<NEO::ConstStringRef> enabledAcronyms{};
    for (const auto &[acronym, ipVersion] : data.acronyms) {
        enabledAcronyms.push_back(acronym.c_str());
    }
    return enabledAcronyms;
}
