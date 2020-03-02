/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cctype>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#pragma once

struct Source {
    const uint8_t *data;
    const size_t length;
    const char *name;
    Source(const uint8_t *data, const size_t length, const char *name)
        : data(data), length(length), name(name){};
    void toVectorOfStrings(std::vector<std::string> &lines, bool replaceTabs = false);
    inline std::vector<char> toBinaryVector() {
        return std::vector<char>(data, data + length);
    };
};

struct Output {
    std::string name;
    uint8_t *data;
    const size_t size;
    Output(const std::string &name, const void *data, const size_t &size);
};

class OclocArgHelper {
  protected:
    std::vector<Source> inputs, headers;
    std::vector<Output *> outputs;
    uint32_t *numOutputs = nullptr;
    char ***nameOutputs = nullptr;
    uint8_t ***dataOutputs = nullptr;
    uint64_t **lenOutputs = nullptr;
    bool hasOutput = false;
    void moveOutputs();

    Source *findSourceFile(const std::string &filename);
    bool sourceFileExists(const std::string &filename) const;

    inline void addOutput(const std::string &filename, const void *data, const size_t &size) {
        outputs.push_back(new Output(filename, data, size));
    }

  public:
    OclocArgHelper() = default;
    OclocArgHelper(const uint32_t numSources, const uint8_t **dataSources,
                   const uint64_t *lenSources, const char **nameSources,
                   const uint32_t numInputHeaders,
                   const uint8_t **dataInputHeaders,
                   const uint64_t *lenInputHeaders, const char **nameInputHeaders,
                   uint32_t *numOutputs, uint8_t ***dataOutputs,
                   uint64_t **lenOutputs, char ***nameOutputs);
    virtual ~OclocArgHelper();

    MOCKABLE_VIRTUAL bool fileExists(const std::string &filename) const;

    std::vector<std::string> headersToVectorOfStrings();
    void readFileToVectorOfStrings(const std::string &filename, std::vector<std::string> &lines);
    MOCKABLE_VIRTUAL std::vector<char> readBinaryFile(const std::string &filename);
    std::unique_ptr<char[]> loadDataFromFile(const std::string &filename, size_t &retSize);

    inline bool outputEnabled() { return hasOutput; }
    inline bool hasHeaders() { return headers.size() > 0; }
    void saveOutput(const std::string &filename, const void *pData, const size_t &dataSize);
    void saveOutput(const std::string &filename, std::ostream &stream);

    void addInput(const std::string &filename, std::stringstream &stream);
};
