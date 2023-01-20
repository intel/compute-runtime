/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"

#include "device_ids_configs.h"
#include "hw_cmds.h"
#include "platforms.h"

ProductConfigHelper::ProductConfigHelper() : deviceAotInfo({
#define DEVICE_CONFIG(productConfig, hwConfig, deviceIds, family, release) {{AOT::productConfig}, &NEO::hwConfig::hwInfo, &NEO::deviceIds, AOT::family, AOT::release},
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

bool ProductConfigHelper::getDeviceAotInfoForProductConfig(AOT::PRODUCT_CONFIG config, DeviceAotInfo &out) const {
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

NEO::ConstStringRef ProductConfigHelper::getAcronymForAFamily(AOT::FAMILY family) {
    for (const auto &[acronym, value] : AOT::familyAcronyms) {
        if (value == family) {
            return NEO::ConstStringRef(acronym);
        }
    }
    return {};
}

AOT::RELEASE ProductConfigHelper::getReleaseForAcronym(const std::string &device) {
    auto it = std::find_if(AOT::releaseAcronyms.begin(), AOT::releaseAcronyms.end(), findMapAcronymWithoutDash(device));
    if (it == AOT::releaseAcronyms.end())
        return AOT::UNKNOWN_RELEASE;
    return it->second;
}

AOT::FAMILY ProductConfigHelper::getFamilyForAcronym(const std::string &device) {
    auto it = std::find_if(AOT::familyAcronyms.begin(), AOT::familyAcronyms.end(), findMapAcronymWithoutDash(device));
    if (it == AOT::familyAcronyms.end())
        return AOT::UNKNOWN_FAMILY;
    return it->second;
}

bool ProductConfigHelper::isRelease(const std::string &device) {
    auto release = getReleaseForAcronym(device);
    if (release == AOT::UNKNOWN_RELEASE) {
        return false;
    }
    return std::any_of(deviceAotInfo.begin(), deviceAotInfo.end(), findRelease(release));
}

bool ProductConfigHelper::isFamily(const std::string &device) {
    auto family = getFamilyForAcronym(device);
    if (family == AOT::UNKNOWN_FAMILY) {
        return false;
    }
    return std::any_of(deviceAotInfo.begin(), deviceAotInfo.end(), findFamily(family));
}

bool ProductConfigHelper::isProductConfig(const std::string &device) {
    auto config = AOT::UNKNOWN_ISA;
    if (device.find(".") != std::string::npos) {
        config = getProductConfigForVersionValue(device);
    } else {
        config = getProductConfigForAcronym(device);
    }

    if (config == AOT::UNKNOWN_ISA) {
        return false;
    }
    return std::any_of(deviceAotInfo.begin(), deviceAotInfo.end(), findProductConfig(config));
}

std::vector<NEO::ConstStringRef> ProductConfigHelper::getRepresentativeProductAcronyms() {
    std::vector<NEO::ConstStringRef> enabledAcronyms{};
    for (const auto &device : deviceAotInfo) {
        if (!device.acronyms.empty()) {
            enabledAcronyms.push_back(device.acronyms.front());
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
        allSupportedAcronyms.insert(allSupportedAcronyms.end(), device.acronyms.begin(), device.acronyms.end());
    }
    return allSupportedAcronyms;
}

PRODUCT_FAMILY ProductConfigHelper::getProductFamilyForAcronym(const std::string &device) const {
    std::vector<DeviceAotInfo>::const_iterator it;
    if (device.find(".") != std::string::npos) {
        it = std::find_if(deviceAotInfo.begin(), deviceAotInfo.end(), findProductConfig(getProductConfigForVersionValue(device)));
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

AOT::PRODUCT_CONFIG ProductConfigHelper::getProductConfigForVersionValue(const std::string &device) {
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

    return static_cast<AOT::PRODUCT_CONFIG>(product.value);
}
