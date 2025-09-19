/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"

#include "shared/source/helpers/hw_info.h"

#include "device_ids_configs.h"
#include "hw_cmds.h"
#include "neo_aot_platforms.h"

ProductConfigHelper::ProductConfigHelper() : deviceAotInfo({
#define DEVICE_CONFIG(productConfig, hwConfig, deviceIds, family, release) {{AOT::productConfig}, &NEO::hwConfig::hwInfo, &NEO::deviceIds, AOT::family, AOT::release, {}, {}},
#include "product_config.inl"
#undef DEVICE_CONFIG
                                             }) {
    std::sort(deviceAotInfo.begin(), deviceAotInfo.end(), compareConfigs);
    initialize();
}

bool ProductConfigHelper::compareConfigs(DeviceAotInfo deviceAotInfo0, DeviceAotInfo deviceAotInfo1) {
    return deviceAotInfo0.aotConfig.value < deviceAotInfo1.aotConfig.value;
}

std::vector<DeviceAotInfo> &ProductConfigHelper::getDeviceAotInfo() {
    return deviceAotInfo;
}

bool ProductConfigHelper::getDeviceAotInfoForProductConfig(uint32_t config, DeviceAotInfo &out) const {
    auto ret = std::find_if(deviceAotInfo.begin(), deviceAotInfo.end(), findProductConfig(config));
    if (ret == deviceAotInfo.end()) {
        return false;
    }
    out = *ret;
    return true;
}

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

void ProductConfigHelper::adjustClosedRangeDeviceLegacyAcronyms(std::string &rangeFromStr, std::string &rangeToStr) {
    // gen12lp is allowed for backwards compatibility, but it only functions as a release.
    // When gen12lp is used in a range with a family this function translates the family to a matching release
    bool isGen12lpAcronymPresent = (rangeFromStr == "gen12lp" || rangeToStr == "gen12lp");
    if (isGen12lpAcronymPresent) {
        auto &allSuppportedProducts = getDeviceAotInfo();

        auto adjustFamilyAcronymToRelease = [&](std::string &device) {
            AOT::FAMILY family = getFamilyFromDeviceName(device);
            if (family == AOT::UNKNOWN_FAMILY)
                return;
            auto latestReleaseInFamily = AOT::UNKNOWN_RELEASE;
            for (const auto &product : allSuppportedProducts) {
                if (product.family == family) {
                    latestReleaseInFamily = std::max(product.release, latestReleaseInFamily);
                }
            }
            device = getAcronymFromARelease(latestReleaseInFamily).str();
        };

        adjustFamilyAcronymToRelease(rangeFromStr);
        adjustFamilyAcronymToRelease(rangeToStr);
    }
}

NEO::ConstStringRef ProductConfigHelper::getAcronymFromAFamily(AOT::FAMILY family) {
    for (const auto &[acronym, value] : AOT::familyAcronyms) {
        if (value == family) {
            return NEO::ConstStringRef(acronym);
        }
    }
    return {};
}

NEO::ConstStringRef ProductConfigHelper::getAcronymFromARelease(AOT::RELEASE release) {
    for (const auto &[acronym, value] : AOT::releaseAcronyms) {
        if (value == release) {
            return NEO::ConstStringRef(acronym);
        }
    }
    return {};
}

AOT::RELEASE ProductConfigHelper::getReleaseFromDeviceName(const std::string &device) const {
    auto it = std::find_if(AOT::releaseAcronyms.begin(), AOT::releaseAcronyms.end(), findMapAcronymWithoutDash(device));
    if (it == AOT::releaseAcronyms.end())
        return AOT::UNKNOWN_RELEASE;
    return isSupportedRelease(it->second) ? it->second : AOT::UNKNOWN_RELEASE;
}

AOT::FAMILY ProductConfigHelper::getFamilyFromDeviceName(const std::string &device) const {
    auto it = std::find_if(AOT::familyAcronyms.begin(), AOT::familyAcronyms.end(), findMapAcronymWithoutDash(device));
    if (it == AOT::familyAcronyms.end())
        return AOT::UNKNOWN_FAMILY;
    return isSupportedFamily(it->second) ? it->second : AOT::UNKNOWN_FAMILY;
}

const std::string ProductConfigHelper::getAcronymForProductConfig(uint32_t config) const {
    auto it = std::find_if(deviceAotInfo.begin(), deviceAotInfo.end(), findProductConfig(config));
    if (it == deviceAotInfo.end()) {
        return {};
    }
    if (!it->deviceAcronyms.empty()) {
        return it->deviceAcronyms.front().str();
    } else if (!it->rtlIdAcronyms.empty()) {
        return it->rtlIdAcronyms.front().str();
    } else
        return parseMajorMinorRevisionValue(it->aotConfig);
}

bool ProductConfigHelper::isSupportedFamily(uint32_t family) const {
    if (family == AOT::UNKNOWN_FAMILY) {
        return false;
    }
    return std::any_of(deviceAotInfo.begin(), deviceAotInfo.end(), findFamily(family));
}

bool ProductConfigHelper::isSupportedRelease(uint32_t release) const {
    if (release == AOT::UNKNOWN_RELEASE) {
        return false;
    }
    return std::any_of(deviceAotInfo.begin(), deviceAotInfo.end(), findRelease(release));
}

bool ProductConfigHelper::isSupportedProductConfig(uint32_t config) const {
    if (config == AOT::UNKNOWN_ISA) {
        return false;
    }
    return std::any_of(deviceAotInfo.begin(), deviceAotInfo.end(), findProductConfig(config));
}

AOT::PRODUCT_CONFIG ProductConfigHelper::getProductConfigFromDeviceName(const std::string &device) const {
    uint32_t config = AOT::UNKNOWN_ISA;
    if (device.find(".") != std::string::npos) {
        config = getProductConfigFromVersionValue(device);
    } else if (std::all_of(device.begin(), device.end(), (::isdigit))) {
        config = static_cast<uint32_t>(std::stoul(device));
    } else {
        config = getProductConfigFromAcronym(device);
    }

    adjustProductConfig(config);
    if (!isSupportedProductConfig(config)) {
        return AOT::UNKNOWN_ISA;
    }

    return static_cast<AOT::PRODUCT_CONFIG>(config);
}

std::vector<NEO::ConstStringRef> ProductConfigHelper::getRepresentativeProductAcronyms() {
    std::vector<NEO::ConstStringRef> enabledAcronyms{};
    for (const auto &device : deviceAotInfo) {
        if (!device.deviceAcronyms.empty()) {
            enabledAcronyms.push_back(device.deviceAcronyms.front());
        } else if (!device.rtlIdAcronyms.empty()) {
            enabledAcronyms.push_back(device.rtlIdAcronyms.front());
        }
    }

    return enabledAcronyms;
}

std::vector<NEO::ConstStringRef> ProductConfigHelper::getReleasesAcronyms() {
    std::vector<NEO::ConstStringRef> ret;
    for (const auto &[acronym, value] : AOT::releaseAcronyms) {
        if (std::any_of(deviceAotInfo.begin(), deviceAotInfo.end(), findRelease(value))) {
            ret.push_back(NEO::ConstStringRef(acronym));
        }
    }
    return ret;
}

std::vector<NEO::ConstStringRef> ProductConfigHelper::getFamiliesAcronyms() {
    std::vector<NEO::ConstStringRef> enabledAcronyms;
    for (const auto &[acronym, value] : AOT::familyAcronyms) {
        if (std::any_of(deviceAotInfo.begin(), deviceAotInfo.end(), findFamily(value))) {
            enabledAcronyms.push_back(NEO::ConstStringRef(acronym));
        }
    }
    return enabledAcronyms;
}

std::vector<NEO::ConstStringRef> ProductConfigHelper::getDeviceAcronyms() {
    std::vector<NEO::ConstStringRef> allSupportedAcronyms{};
    for (const auto &device : AOT::deviceAcronyms) {
        allSupportedAcronyms.push_back(NEO::ConstStringRef(device.first));
    }
    return allSupportedAcronyms;
}

std::vector<NEO::ConstStringRef> ProductConfigHelper::getAllProductAcronyms() {
    std::vector<NEO::ConstStringRef> allSupportedAcronyms{};
    for (const auto &device : deviceAotInfo) {
        allSupportedAcronyms.insert(allSupportedAcronyms.end(), device.deviceAcronyms.begin(), device.deviceAcronyms.end());
        allSupportedAcronyms.insert(allSupportedAcronyms.end(), device.rtlIdAcronyms.begin(), device.rtlIdAcronyms.end());
    }
    return allSupportedAcronyms;
}

PRODUCT_FAMILY ProductConfigHelper::getProductFamilyFromDeviceName(const std::string &device) const {
    std::vector<DeviceAotInfo>::const_iterator it;
    if (device.find(".") != std::string::npos) {
        it = std::find_if(deviceAotInfo.begin(), deviceAotInfo.end(), findProductConfig(getProductConfigFromVersionValue(device)));
    } else {
        it = std::find_if(deviceAotInfo.begin(), deviceAotInfo.end(), findAcronym(device));
    }
    if (it != deviceAotInfo.end())
        return it->hwInfo->platform.eProductFamily;
    return IGFX_UNKNOWN;
}

std::vector<NEO::ConstStringRef> ProductConfigHelper::getDeprecatedAcronyms() {
    std::vector<NEO::ConstStringRef> prefixes{}, deprecatedAcronyms{};
    for (int j = 0; j < IGFX_MAX_PRODUCT; j++) {
        if (NEO::hardwarePrefix[j] == nullptr)
            continue;
        prefixes.push_back(NEO::hardwarePrefix[j]);
    }

    for (const auto &prefix : prefixes) {
        std::string prefixCopy = prefix.str();
        ProductConfigHelper::adjustDeviceName(prefixCopy);

        if (std::any_of(deviceAotInfo.begin(), deviceAotInfo.end(), findAcronym(prefixCopy)))
            continue;
        deprecatedAcronyms.push_back(prefix);
    }
    return deprecatedAcronyms;
}

std::string ProductConfigHelper::parseMajorMinorRevisionValue(NEO::HardwareIpVersion config) {
    std::stringstream stringConfig;
    stringConfig << config.architecture << "." << config.release << "." << config.revision;
    return stringConfig.str();
}

std::string ProductConfigHelper::parseMajorMinorValue(NEO::HardwareIpVersion config) {
    std::stringstream stringConfig;
    stringConfig << config.architecture << "." << config.release;
    return stringConfig.str();
}

int ProductConfigHelper::parseProductConfigFromString(const std::string &device, size_t begin, size_t end) {
    if (begin == end) {
        return ConfigStatus::MismatchedValue;
    }
    if (end == std::string::npos) {
        if (!std::all_of(device.begin() + begin, device.end(), (::isdigit))) {
            return ConfigStatus::MismatchedValue;
        }
        return std::stoi(device.substr(begin, device.size() - begin));
    } else {
        if (!std::all_of(device.begin() + begin, device.begin() + end, (::isdigit))) {
            return ConfigStatus::MismatchedValue;
        }
        return std::stoi(device.substr(begin, end - begin));
    }
}

uint32_t ProductConfigHelper::getProductConfigFromVersionValue(const std::string &device) {
    auto majorPos = device.find(".");
    auto major = parseProductConfigFromString(device, 0, majorPos);
    if (major == ConfigStatus::MismatchedValue || majorPos == std::string::npos) {
        return AOT::UNKNOWN_ISA;
    }

    auto minorPos = device.find(".", ++majorPos);
    auto minor = parseProductConfigFromString(device, majorPos, minorPos);

    if (minor == ConfigStatus::MismatchedValue || minorPos == std::string::npos) {
        return AOT::UNKNOWN_ISA;
    }

    auto revision = parseProductConfigFromString(device, minorPos + 1, device.size());
    if (revision == ConfigStatus::MismatchedValue) {
        return AOT::UNKNOWN_ISA;
    }

    NEO::HardwareIpVersion product = {0};
    product.architecture = major;
    product.release = minor;
    product.revision = revision;

    return product.value;
}
void ProductConfigHelper::initialize() {
    for (auto &device : deviceAotInfo) {
        for (const auto &[acronym, value] : AOT::deviceAcronyms) {
            if (value == device.aotConfig.value) {
                device.deviceAcronyms.push_back(NEO::ConstStringRef(acronym));
            }
        }

        for (const auto &[acronym, value] : AOT::getRtlIdAcronyms()) {
            if (value == device.aotConfig.value) {
                device.rtlIdAcronyms.push_back(NEO::ConstStringRef(acronym));
            }
        }

        for (const auto &[acronym, value] : AOT::genericIdAcronyms) {
            if (value == device.aotConfig.value) {
                device.deviceAcronyms.push_back(NEO::ConstStringRef(acronym));
            }
        }
    }
}

AOT::PRODUCT_CONFIG ProductConfigHelper::getProductConfigFromAcronym(const std::string &device) {
    auto deviceAcronymIt = std::find_if(AOT::deviceAcronyms.begin(), AOT::deviceAcronyms.end(), findMapAcronymWithoutDash(device));
    if (deviceAcronymIt != AOT::deviceAcronyms.end()) {
        return deviceAcronymIt->second;
    }

    auto rtlIdAcronymIt = std::find_if(AOT::getRtlIdAcronyms().begin(), AOT::getRtlIdAcronyms().end(), findMapAcronymWithoutDash(device));
    if (rtlIdAcronymIt != AOT::getRtlIdAcronyms().end()) {
        return rtlIdAcronymIt->second;
    }

    auto genericIdAcronymIt = std::find_if(AOT::genericIdAcronyms.begin(), AOT::genericIdAcronyms.end(), findMapAcronymWithoutDash(device));
    if (genericIdAcronymIt != AOT::genericIdAcronyms.end()) {
        return genericIdAcronymIt->second;
    }
    return AOT::UNKNOWN_ISA;
}
