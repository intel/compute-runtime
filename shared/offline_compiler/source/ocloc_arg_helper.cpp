/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocloc_arg_helper.h"

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
#define DEVICE_CONFIG_IDS_AND_REVISION(product, productConfig, deviceIds, revision_id) {product, &NEO::productConfig::hwInfo, &NEO::deviceIds, NEO::productConfig::setupFeatureAndWorkaroundTable, revision_id},
#define DEVICE_CONFIG_IDS(product, productConfig, deviceIds) {product, &NEO::productConfig::hwInfo, &NEO::deviceIds, NEO::productConfig::setupFeatureAndWorkaroundTable, NEO::productConfig::hwInfo.platform.usRevId},
#define DEVICE_CONFIG(product, productConfig) {product, &NEO::productConfig::hwInfo, nullptr, NEO::productConfig::setupFeatureAndWorkaroundTable, NEO::productConfig::hwInfo.platform.usRevId},
#include "product_config.inl"
#undef DEVICE_CONFIG
#undef DEVICE_CONFIG_IDS
#undef DEVICE_CONFIG_IDS_AND_REVISION
      }) {
    for (uint32_t i = 0; i < numSources; ++i) {
        inputs.push_back(Source(dataSources[i], static_cast<size_t>(lenSources[i]), nameSources[i]));
    }
    for (uint32_t i = 0; i < numInputHeaders; ++i) {
        headers.push_back(Source(dataInputHeaders[i], static_cast<size_t>(lenInputHeaders[i]), nameInputHeaders[i]));
    }
    for (unsigned int family = 0; family < IGFX_MAX_CORE; ++family) {
        if (NEO::familyName[family] == nullptr) {
            continue;
        }
        insertGenNames(static_cast<GFXCORE_FAMILY>(family));
    }

    std::sort(deviceMap.begin(), deviceMap.end(), compareConfigs);
    deviceMap.erase(std::unique(deviceMap.begin(), deviceMap.end(), isDuplicateConfig), deviceMap.end());
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

void OclocArgHelper::setDeviceInfoForFatbinaryTarget(const DeviceMapping &device) {
    deviceForFatbinary.hwInfo = device.hwInfo;
    deviceForFatbinary.setupFeatureAndWorkaroundTable = device.setupFeatureAndWorkaroundTable;
    deviceForFatbinary.revId = device.revId;
    deviceForFatbinary.deviceIds = device.deviceIds;
}

void OclocArgHelper::setHwInfoForFatbinaryTarget(NEO::HardwareInfo &hwInfo) {
    hwInfo = *deviceForFatbinary.hwInfo;
    deviceForFatbinary.setupFeatureAndWorkaroundTable(&hwInfo);
    hwInfo.platform.usRevId = deviceForFatbinary.revId;
    if (deviceForFatbinary.deviceIds) {
        hwInfo.platform.usDeviceID = deviceForFatbinary.deviceIds->front();
    }
}

bool OclocArgHelper::getHwInfoForProductConfig(uint32_t config, NEO::HardwareInfo &hwInfo) {
    bool retVal = false;
    if (config == UNKNOWN_ISA) {
        return retVal;
    }
    for (auto &deviceConfig : deviceMap) {
        if (deviceConfig.config == config) {
            hwInfo = *deviceConfig.hwInfo;
            deviceConfig.setupFeatureAndWorkaroundTable(&hwInfo);
            hwInfo.platform.usRevId = deviceConfig.revId;
            if (deviceConfig.deviceIds) {
                hwInfo.platform.usDeviceID = deviceConfig.deviceIds->front();
            }
            retVal = true;
            return retVal;
        }
    }
    return retVal;
}

void OclocArgHelper::getProductConfigsForGfxCoreFamily(GFXCORE_FAMILY core, std::vector<DeviceMapping> &out) {
    for (auto &deviceConfig : deviceMap) {
        if (deviceConfig.hwInfo->platform.eRenderCoreFamily == core) {
            out.push_back(deviceConfig);
        }
    }
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

const std::string OclocArgHelper::parseProductConfigFromValue(PRODUCT_CONFIG config) {
    auto configValue = static_cast<uint32_t>(config);
    std::stringstream stringConfig;
    uint32_t major = (configValue & 0xff0000) >> 16;
    uint32_t minor = (configValue & 0x00ff00) >> 8;
    uint32_t revision = configValue & 0x0000ff;
    stringConfig << major << "." << minor << "." << revision;
    return stringConfig.str();
}

std::vector<PRODUCT_CONFIG> OclocArgHelper::getAllSupportedProductConfigs() {
    std::vector<PRODUCT_CONFIG> allConfigs;
    for (auto &deviceConfig : deviceMap) {
        allConfigs.push_back(deviceConfig.config);
    }
    std::sort(allConfigs.begin(), allConfigs.end());
    return allConfigs;
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

std::vector<uint32_t> OclocArgHelper::getMajorMinorRevision(const std::string &device) {
    std::vector<uint32_t> numeration;
    auto major_pos = device.find(".");
    auto major = parseProductConfigFromString(device, 0, major_pos);
    if (major == CONFIG_STATUS::MISMATCHED_VALUE) {
        return {};
    }
    numeration.push_back(major);
    if (major_pos == std::string::npos) {
        return numeration;
    }

    auto minor_pos = device.find(".", ++major_pos);
    auto minor = parseProductConfigFromString(device, major_pos, minor_pos);

    if (minor == CONFIG_STATUS::MISMATCHED_VALUE) {
        return {};
    }
    numeration.push_back(minor);
    if (minor_pos == std::string::npos) {
        return numeration;
    }
    auto revision = parseProductConfigFromString(device, minor_pos + 1, device.size());
    if (revision == CONFIG_STATUS::MISMATCHED_VALUE) {
        return {};
    }
    numeration.push_back(revision);
    return numeration;
}

uint32_t OclocArgHelper::getProductConfig(std::vector<uint32_t> &numeration) {
    uint32_t config = 0x0;

    config = numeration.at(0) << 16;
    if (numeration.size() > 1) {
        config |= (numeration.at(1) << 8);
    }
    if (numeration.size() > 2) {
        config |= numeration.at(2);
    }

    return config;
}

uint32_t OclocArgHelper::getMaskForConfig(std::vector<uint32_t> &numeration) {
    uint32_t mask = 0xffffff;
    if (numeration.size() == 1) {
        mask = 0xff0000;
    } else if (numeration.size() == 2) {
        mask = 0xffff00;
    }
    return mask;
}

bool OclocArgHelper::isGen(const std::string &device) {
    std::string buf(device);
    std::transform(buf.begin(), buf.end(), buf.begin(), ::tolower);
    auto it = genIGFXMap.find(buf);
    return it == genIGFXMap.end() ? false : true;
}

unsigned int OclocArgHelper::returnIGFXforGen(const std::string &device) {
    std::string buf(device);
    std::transform(buf.begin(), buf.end(), buf.begin(), ::tolower);
    auto it = genIGFXMap.find(buf);
    if (it == genIGFXMap.end())
        return 0;
    return it->second;
}

bool OclocArgHelper::areQuotesRequired(const std::string_view &argName) {
    return argName == "-options" || argName == "-internal_options";
}

PRODUCT_CONFIG OclocArgHelper::findConfigMatch(const std::string &device, bool firstAppearance) {
    auto numeration = getMajorMinorRevision(device);
    if (numeration.empty()) {
        return PRODUCT_CONFIG::UNKNOWN_ISA;
    }

    std::vector<PRODUCT_CONFIG> allMatchedConfigs;
    std::vector<PRODUCT_CONFIG> allConfigs = getAllSupportedProductConfigs();
    auto configValue = getProductConfig(numeration);
    uint32_t mask = getMaskForConfig(numeration);

    if (!firstAppearance) {
        // find last appearance
        std::reverse(allConfigs.begin(), allConfigs.end());
    }

    for (auto &productConfig : allConfigs) {
        uint32_t value = static_cast<uint32_t>(productConfig) & mask;
        if (value == configValue) {
            return productConfig;
        }
    }
    return PRODUCT_CONFIG::UNKNOWN_ISA;
}

void OclocArgHelper::insertGenNames(GFXCORE_FAMILY family) {
    std::string genName = NEO::familyName[family];
    std::transform(genName.begin(), genName.end(), genName.begin(), ::tolower);
    genIGFXMap.insert({genName, family});

    auto findCore = genName.find("_core");
    if (findCore != std::string::npos) {
        genName = genName.substr(0, findCore);
        genIGFXMap.insert({genName, family});
    }

    auto findUnderline = genName.find("_");
    if (findUnderline != std::string::npos) {
        genName.erase(std::remove(genName.begin(), genName.end(), '_'), genName.end());
        genIGFXMap.insert({genName, family});
    }
}
