/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocloc_arg_helper.h"

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"

#include "hw_cmds.h"

#include <algorithm>
#include <cstring>
#include <sstream>

void Source::toVectorOfStrings(std::vector<std::string> &lines, bool replaceTabs) {
    std::string line;
    const char *file = reinterpret_cast<const char *>(data);

    while (*file != '\0') {
        if (replaceTabs && *file == '\t') {
            line += ' ';
        } else if (*file == '\n') {
            if (!line.empty()) {
                lines.push_back(line);
                line = "";
            }
        } else {
            line += *file;
        }
        file++;
    }

    if (!line.empty()) {
        lines.push_back(std::move(line));
    }
}

Output::Output(const std::string &name, const void *data, const size_t &size)
    : name(name), size(size) {
    this->data = new uint8_t[size];
    memcpy_s(reinterpret_cast<void *>(this->data), this->size, data, size);
};

OclocArgHelper::OclocArgHelper(const uint32_t numSources, const uint8_t **dataSources,
                               const uint64_t *lenSources, const char **nameSources,
                               const uint32_t numInputHeaders,
                               const uint8_t **dataInputHeaders,
                               const uint64_t *lenInputHeaders, const char **nameInputHeaders,
                               uint32_t *numOutputs, uint8_t ***dataOutputs,
                               uint64_t **lenOutputs, char ***nameOutputs)
    : numOutputs(numOutputs), nameOutputs(nameOutputs),
      dataOutputs(dataOutputs), lenOutputs(lenOutputs), hasOutput(numOutputs != nullptr),
      messagePrinter(hasOutput), deviceProductTable({
#define NAMEDDEVICE(devId, product, ignored_devName) {devId, NEO::hardwarePrefix[NEO::product::hwInfo.platform.eProductFamily]},
#define DEVICE(devId, product) {devId, NEO::hardwarePrefix[NEO::product::hwInfo.platform.eProductFamily]},
#include "devices.inl"
#undef DEVICE
#undef NAMEDDEVICE
                                     {0u, std::string("")}}),
      deviceMap({
#define DEVICE_CONFIG_IDS(product, productConfig, deviceIds, family, release) {&NEO::productConfig::hwInfo, &NEO::deviceIds, AOT::family, AOT::release, {AOT::product}},
#define DEVICE_CONFIG(product, productConfig, family, release) {&NEO::productConfig::hwInfo, nullptr, AOT::family, AOT::release, {AOT::product}},
#include "product_config.inl"
#undef DEVICE_CONFIG
#undef DEVICE_CONFIG_IDS
      }) {
    for (uint32_t i = 0; i < numSources; ++i) {
        inputs.push_back(Source(dataSources[i], static_cast<size_t>(lenSources[i]), nameSources[i]));
    }
    for (uint32_t i = 0; i < numInputHeaders; ++i) {
        headers.push_back(Source(dataInputHeaders[i], static_cast<size_t>(lenInputHeaders[i]), nameInputHeaders[i]));
    }

    std::sort(deviceMap.begin(), deviceMap.end(), compareConfigs);
    for (auto &device : deviceMap) {
        for (const auto &[acronym, value] : AOT::productConfigAcronyms) {
            if (value == device.aotConfig.ProductConfig) {
                device.acronyms.push_back(NEO::ConstStringRef(acronym));
            }
        }
    }
}

OclocArgHelper::OclocArgHelper() : OclocArgHelper(0, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) {}

OclocArgHelper::~OclocArgHelper() {
    if (outputEnabled()) {
        saveOutput(oclocStdoutLogName, messagePrinter.getLog());
        moveOutputs();
    }
}

bool OclocArgHelper::fileExists(const std::string &filename) const {
    return sourceFileExists(filename) || ::fileExists(filename);
}

void OclocArgHelper::moveOutputs() {
    *numOutputs = static_cast<uint32_t>(outputs.size());
    *nameOutputs = new char *[outputs.size()];
    *dataOutputs = new uint8_t *[outputs.size()];
    *lenOutputs = new uint64_t[outputs.size()];
    for (size_t i = 0; i < outputs.size(); ++i) {
        size_t size = outputs[i]->name.length() + 1;
        (*nameOutputs)[i] = new char[size];
        strncpy_s((*nameOutputs)[i], size, outputs[i]->name.c_str(), outputs[i]->name.length() + 1);
        (*dataOutputs)[i] = outputs[i]->data;
        (*lenOutputs)[i] = outputs[i]->size;
    }
}

Source *OclocArgHelper::findSourceFile(const std::string &filename) {
    for (auto &source : inputs) {
        if (filename == source.name) {
            return &source;
        }
    }
    return nullptr;
}

bool OclocArgHelper::sourceFileExists(const std::string &filename) const {
    for (auto &input : inputs) {
        if (filename == input.name) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> OclocArgHelper::headersToVectorOfStrings() {
    std::vector<std::string> lines;
    for (auto &header : headers) {
        header.toVectorOfStrings(lines, true);
    }
    return lines;
}

void OclocArgHelper::readFileToVectorOfStrings(const std::string &filename, std::vector<std::string> &lines) {
    if (Source *s = findSourceFile(filename)) {
        s->toVectorOfStrings(lines);
    } else {
        ::readFileToVectorOfStrings(lines, filename);
    }
}

std::vector<char> OclocArgHelper::readBinaryFile(const std::string &filename) {
    if (Source *s = findSourceFile(filename)) {
        return s->toBinaryVector();
    } else {
        return ::readBinaryFile(filename);
    }
}

std::unique_ptr<char[]> OclocArgHelper::loadDataFromFile(const std::string &filename, size_t &retSize) {
    if (Source *s = findSourceFile(filename)) {
        auto size = s->length;
        std::unique_ptr<char[]> ret(new char[size]());
        memcpy_s(ret.get(), size, s->data, s->length);
        retSize = s->length;
        return ret;
    } else {
        return ::loadDataFromFile(filename.c_str(), retSize);
    }
}

bool OclocArgHelper::getHwInfoForProductConfig(uint32_t config, NEO::HardwareInfo &hwInfo, uint64_t hwInfoConfig) {
    bool retVal = false;
    if (config == AOT::UNKNOWN_ISA) {
        return retVal;
    }

    for (auto &deviceConfig : deviceMap) {
        if (deviceConfig.aotConfig.ProductConfig == config) {
            hwInfo = *deviceConfig.hwInfo;
            const auto &compilerHwInfoConfig = *NEO::CompilerHwInfoConfig::get(hwInfo.platform.eProductFamily);
            compilerHwInfoConfig.setProductConfigForHwInfo(hwInfo, deviceConfig.aotConfig);
            if (hwInfoConfig) {
                setHwInfoValuesFromConfig(hwInfoConfig, hwInfo);
            }
            NEO::hardwareInfoBaseSetup[hwInfo.platform.eProductFamily](&hwInfo, true);

            if (deviceConfig.deviceIds) {
                hwInfo.platform.usDeviceID = deviceConfig.deviceIds->front();
            }
            retVal = true;
            return retVal;
        }
    }
    return retVal;
}

std::vector<NEO::ConstStringRef> OclocArgHelper::getDeprecatedAcronyms() {
    std::vector<NEO::ConstStringRef> prefixes{}, deprecatedAcronyms{}, enabledAcronyms{};
    for (int j = 0; j < IGFX_MAX_PRODUCT; j++) {
        if (NEO::hardwarePrefix[j] == nullptr)
            continue;
        prefixes.push_back(NEO::hardwarePrefix[j]);
    }

    for (const auto &device : deviceMap) {
        enabledAcronyms.insert(enabledAcronyms.end(), device.acronyms.begin(), device.acronyms.end());
    }

    for (const auto &prefix : prefixes) {
        if (std::any_of(enabledAcronyms.begin(), enabledAcronyms.end(), ProductConfigHelper::findAcronymWithoutDash(prefix.str())))
            continue;
        deprecatedAcronyms.push_back(prefix);
    }
    return deprecatedAcronyms;
}

void OclocArgHelper::saveOutput(const std::string &filename, const void *pData, const size_t &dataSize) {
    if (outputEnabled()) {
        addOutput(filename, pData, dataSize);
    } else {
        writeDataToFile(filename.c_str(), pData, dataSize);
    }
}

void OclocArgHelper::saveOutput(const std::string &filename, const std::ostream &stream) {
    std::stringstream ss;
    ss << stream.rdbuf();
    if (outputEnabled()) {
        addOutput(filename, ss.str().c_str(), ss.str().length());
    } else {
        std::ofstream file(filename);
        file << ss.str();
    }
}

std::string OclocArgHelper::getAllSupportedAcronyms() {
    std::ostringstream os;
    for (const auto &device : deviceMap) {
        for (const auto &acronym : device.acronyms) {
            if (os.tellp())
                os << ", ";
            os << acronym.str();
        }
    }
    return os.str();
}

std::vector<NEO::ConstStringRef> OclocArgHelper::getEnabledProductAcronyms() {
    std::vector<NEO::ConstStringRef> enabledAcronyms{};
    for (const auto &device : deviceMap) {
        if (!device.acronyms.empty()) {
            enabledAcronyms.push_back(device.acronyms.front());
        }
    }
    return enabledAcronyms;
}

std::vector<NEO::ConstStringRef> OclocArgHelper::getEnabledReleasesAcronyms() {
    std::vector<NEO::ConstStringRef> ret;
    for (const auto &[acronym, value] : AOT::releaseAcronyms) {
        if (std::any_of(deviceMap.begin(), deviceMap.end(), findRelease(value))) {
            ret.push_back(NEO::ConstStringRef(acronym));
        }
    }
    return ret;
}

std::vector<NEO::ConstStringRef> OclocArgHelper::getEnabledFamiliesAcronyms() {
    std::vector<NEO::ConstStringRef> enabledAcronyms;
    for (const auto &[acronym, value] : AOT::familyAcronyms) {
        if (std::any_of(deviceMap.begin(), deviceMap.end(), findFamily(value))) {
            enabledAcronyms.push_back(NEO::ConstStringRef(acronym));
        }
    }
    return enabledAcronyms;
}

bool OclocArgHelper::setAcronymForDeviceId(std::string &device) {
    auto product = returnProductNameForDevice(std::stoi(device, 0, 16));
    if (!product.empty()) {
        printf("Auto-detected target based on %s device id: %s\n", device.c_str(), product.c_str());
    } else {
        printf("Could not determine target based on device id: %s\n", device.c_str());
        return false;
    }
    device = std::move(product);
    return true;
}

std::string OclocArgHelper::returnProductNameForDevice(unsigned short deviceId) {
    std::string res = "";
    for (int i = 0; deviceProductTable[i].deviceId != 0; i++) {
        if (deviceProductTable[i].deviceId == deviceId) {
            res = deviceProductTable[i].product;
        }
    }
    return res;
}

std::vector<DeviceMapping> &OclocArgHelper::getAllSupportedDeviceConfigs() {
    return deviceMap;
}

int OclocArgHelper::parseProductConfigFromString(const std::string &device, size_t begin, size_t end) {
    if (begin == end) {
        return CONFIG_STATUS::MISMATCHED_VALUE;
    }
    if (end == std::string::npos) {
        if (!std::all_of(device.begin() + begin, device.end(), (::isdigit))) {
            return CONFIG_STATUS::MISMATCHED_VALUE;
        }
        return std::stoi(device.substr(begin, device.size() - begin));
    } else {
        if (!std::all_of(device.begin() + begin, device.begin() + end, (::isdigit))) {
            return CONFIG_STATUS::MISMATCHED_VALUE;
        }
        return std::stoi(device.substr(begin, end - begin));
    }
}

AOT::PRODUCT_CONFIG OclocArgHelper::getProductConfigForVersionValue(const std::string &device) {
    auto majorPos = device.find(".");
    auto major = parseProductConfigFromString(device, 0, majorPos);
    if (major == CONFIG_STATUS::MISMATCHED_VALUE || majorPos == std::string::npos) {
        return AOT::UNKNOWN_ISA;
    }
    auto minorPos = device.find(".", ++majorPos);
    auto minor = parseProductConfigFromString(device, majorPos, minorPos);
    if (minor == CONFIG_STATUS::MISMATCHED_VALUE || minorPos == std::string::npos) {
        return AOT::UNKNOWN_ISA;
    }
    auto revision = parseProductConfigFromString(device, minorPos + 1, device.size());
    if (revision == CONFIG_STATUS::MISMATCHED_VALUE) {
        return AOT::UNKNOWN_ISA;
    }
    AheadOfTimeConfig product = {0};
    product.ProductConfigID.Major = major;
    product.ProductConfigID.Minor = minor;
    product.ProductConfigID.Revision = revision;
    return static_cast<AOT::PRODUCT_CONFIG>(product.ProductConfig);
}

bool OclocArgHelper::isRelease(const std::string &device) {
    auto release = ProductConfigHelper::returnReleaseForAcronym(device);
    if (release == AOT::UNKNOWN_RELEASE) {
        return false;
    }
    return std::any_of(deviceMap.begin(), deviceMap.end(), findRelease(release));
}

bool OclocArgHelper::isFamily(const std::string &device) {
    auto family = ProductConfigHelper::returnFamilyForAcronym(device);
    if (family == AOT::UNKNOWN_FAMILY) {
        return false;
    }
    return std::any_of(deviceMap.begin(), deviceMap.end(), findFamily(family));
}

bool OclocArgHelper::isProductConfig(const std::string &device) {
    auto config = AOT::UNKNOWN_ISA;
    if (device.find(".") != std::string::npos) {
        config = getProductConfigForVersionValue(device);
    } else {
        config = ProductConfigHelper::returnProductConfigForAcronym(device);
    }
    if (config == AOT::UNKNOWN_ISA) {
        return false;
    }
    return std::any_of(deviceMap.begin(), deviceMap.end(), findProductConfig(config));
}

bool OclocArgHelper::areQuotesRequired(const std::string_view &argName) {
    return argName == "-options" || argName == "-internal_options";
}
