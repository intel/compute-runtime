/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocloc_arg_helper.h"

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/release_helper/release_helper.h"

#include "hw_cmds.h"
#include "neo_aot_platforms.h"

#include <algorithm>
#include <cstring>
#include <sstream>

void Source::toVectorOfStrings(std::vector<std::string> &lines, bool replaceTabs) {
    std::string line;
    const char *file = reinterpret_cast<const char *>(data);
    const char *end = file + length;

    while (file != end && *file != '\0') {
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
    memcpy_s(this->data, this->size, data, size);
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
      messagePrinter(hasOutput) {
    for (uint32_t i = 0; i < numSources; ++i) {
        inputs.push_back(Source(dataSources[i], static_cast<size_t>(lenSources[i]), nameSources[i]));
    }
    for (uint32_t i = 0; i < numInputHeaders; ++i) {
        headers.push_back(Source(dataInputHeaders[i], static_cast<size_t>(lenInputHeaders[i]), nameInputHeaders[i]));
    }

    productConfigHelper = std::make_unique<ProductConfigHelper>();
}

OclocArgHelper::OclocArgHelper() : OclocArgHelper(0, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) {}

OclocArgHelper::~OclocArgHelper() {
    if (outputEnabled()) {
        auto log = messagePrinter.getLog().str();
        OclocArgHelper::saveOutput(oclocStdoutLogName, log.c_str(), log.length() + 1);
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

uint32_t OclocArgHelper::getProductConfigAndSetHwInfoBasedOnDeviceAndRevId(NEO::HardwareInfo &hwInfo, unsigned short deviceID, int revisionID, std::unique_ptr<NEO::CompilerProductHelper> &compilerProductHelper, std::unique_ptr<NEO::ReleaseHelper> &releaseHelper) {
    const auto &deviceAotMap = productConfigHelper->getDeviceAotInfo();

    for (const auto &device : deviceAotMap) {
        if (std::find(device.deviceIds->begin(), device.deviceIds->end(), deviceID) != device.deviceIds->end()) {
            hwInfo = *device.hwInfo;
            hwInfo.platform.usDeviceID = deviceID;
            if (revisionID != -1) {
                hwInfo.platform.usRevId = revisionID;
                compilerProductHelper = NEO::CompilerProductHelper::create(hwInfo.platform.eProductFamily);
                UNRECOVERABLE_IF(compilerProductHelper == nullptr);
                auto config = compilerProductHelper->matchRevisionIdWithProductConfig(device.aotConfig, revisionID);
                if (productConfigHelper->isSupportedProductConfig(config)) {
                    hwInfo.ipVersion = config;
                    releaseHelper = NEO::ReleaseHelper::create(hwInfo.ipVersion);
                    return config;
                }
            }
            hwInfo.ipVersion = device.aotConfig.value;
            releaseHelper = NEO::ReleaseHelper::create(hwInfo.ipVersion);
            return device.aotConfig.value;
        }
    }
    return AOT::UNKNOWN_ISA;
}

bool OclocArgHelper::setHwInfoForProductConfig(uint32_t productConfig, NEO::HardwareInfo &hwInfo, std::unique_ptr<NEO::CompilerProductHelper> &compilerProductHelper, std::unique_ptr<NEO::ReleaseHelper> &releaseHelper) {
    if (productConfig == AOT::UNKNOWN_ISA) {
        return false;
    }
    const auto &deviceAotMap = productConfigHelper->getDeviceAotInfo();
    for (auto &deviceConfig : deviceAotMap) {
        if (deviceConfig.aotConfig.value == productConfig) {
            hwInfo = *deviceConfig.hwInfo;
            hwInfo.platform.usDeviceID = deviceConfig.deviceIds->front();
            compilerProductHelper = NEO::CompilerProductHelper::create(hwInfo.platform.eProductFamily);
            UNRECOVERABLE_IF(compilerProductHelper == nullptr);
            compilerProductHelper->setProductConfigForHwInfo(hwInfo, productConfig);
            releaseHelper = NEO::ReleaseHelper::create(hwInfo.ipVersion);
            return true;
        }
    }
    return false;
}

void OclocArgHelper::setHwInfoForHwInfoConfig(NEO::HardwareInfo &hwInfo, uint64_t hwInfoConfig, std::unique_ptr<NEO::CompilerProductHelper> &compilerProductHelper, std::unique_ptr<NEO::ReleaseHelper> &releaseHelper) {
    compilerProductHelper = NEO::CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    UNRECOVERABLE_IF(compilerProductHelper == nullptr);
    uint64_t config = hwInfoConfig ? hwInfoConfig : compilerProductHelper->getHwInfoConfig(hwInfo);
    setHwInfoValuesFromConfig(config, hwInfo);
    releaseHelper = NEO::ReleaseHelper::create(hwInfo.ipVersion);
    NEO::hardwareInfoBaseSetup[hwInfo.platform.eProductFamily](&hwInfo, true, releaseHelper.get());
}

void OclocArgHelper::saveOutput(const std::string &filename, const void *pData, const size_t &dataSize) {
    if (outputEnabled()) {
        addOutput(filename, pData, dataSize);
    } else {
        writeDataToFile(filename.c_str(), std::string_view(static_cast<const char *>(pData), dataSize));
    }
}