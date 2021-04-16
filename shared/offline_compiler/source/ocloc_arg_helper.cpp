/*
 * Copyright (C) 2020-2021 Intel Corporation
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
            lines.push_back(line);
            line = "";
        } else {
            line += *file;
        }
        file++;
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
      dataOutputs(dataOutputs), lenOutputs(lenOutputs), hasOutput(numOutputs != nullptr), deviceProductTable({
#define NAMEDDEVICE(devId, product, ignored_gtType, ignored_devName) {devId, NEO::hardwarePrefix[NEO::product::hwInfo.platform.eProductFamily]},
#define DEVICE(devId, product, ignored_gtType) {devId, NEO::hardwarePrefix[NEO::product::hwInfo.platform.eProductFamily]},
#include "devices.inl"
#undef DEVICE
#undef NAMEDDEVICE
                                                                                              {0u, std::string("")}}) {
    for (uint32_t i = 0; i < numSources; ++i) {
        inputs.push_back(Source(dataSources[i], static_cast<size_t>(lenSources[i]), nameSources[i]));
    }
    for (uint32_t i = 0; i < numInputHeaders; ++i) {
        headers.push_back(Source(dataInputHeaders[i], static_cast<size_t>(lenInputHeaders[i]), nameInputHeaders[i]));
    }
    for (unsigned int i = 0; i < IGFX_MAX_CORE; ++i) {
        if (NEO::familyName[i] == nullptr)
            continue;
        std::string gen = NEO::familyName[i];
        std::transform(gen.begin(), gen.end(), gen.begin(), ::tolower);
        genIGFXMap.insert({gen, i});
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