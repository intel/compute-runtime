/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/decoder/helper.h"
#include "shared/source/utilities/const_stringref.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

constexpr auto *oclocStdoutLogName = "stdout.log";

struct ProductConfigHelper;
namespace NEO {
class CompilerProductHelper;
class ReleaseHelper;
struct HardwareInfo;
} // namespace NEO

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
    std::vector<std::unique_ptr<Output>> outputs;
    uint32_t *numOutputs = nullptr;
    char ***nameOutputs = nullptr;
    uint8_t ***dataOutputs = nullptr;
    uint64_t **lenOutputs = nullptr;
    bool hasOutput = false;
    MessagePrinter messagePrinter;
    void moveOutputs();
    Source *findSourceFile(const std::string &filename);
    bool sourceFileExists(const std::string &filename) const;

    inline void addOutput(const std::string &filename, const void *data, const size_t &size) {
        outputs.push_back(std::make_unique<Output>(filename, data, size));
    }

    bool verbose = false;

  public:
    OclocArgHelper();
    OclocArgHelper(const uint32_t numSources, const uint8_t **dataSources,
                   const uint64_t *lenSources, const char **nameSources,
                   const uint32_t numInputHeaders,
                   const uint8_t **dataInputHeaders,
                   const uint64_t *lenInputHeaders, const char **nameInputHeaders,
                   uint32_t *numOutputs, uint8_t ***dataOutputs,
                   uint64_t **lenOutputs, char ***nameOutputs);
    virtual ~OclocArgHelper();
    MOCKABLE_VIRTUAL bool fileExists(const std::string &filename) const;
    bool setHwInfoForProductConfig(uint32_t productConfig, NEO::HardwareInfo &hwInfo, std::unique_ptr<NEO::CompilerProductHelper> &compilerProductHelper, std::unique_ptr<NEO::ReleaseHelper> &releaseHelper);
    uint32_t getProductConfigAndSetHwInfoBasedOnDeviceAndRevId(NEO::HardwareInfo &hwInfo, unsigned short deviceID, int revisionID, std::unique_ptr<NEO::CompilerProductHelper> &compilerProductHelper, std::unique_ptr<NEO::ReleaseHelper> &releaseHelper);
    void setHwInfoForHwInfoConfig(NEO::HardwareInfo &hwInfo, uint64_t hwInfoConfig, std::unique_ptr<NEO::CompilerProductHelper> &compilerProductHelper, std::unique_ptr<NEO::ReleaseHelper> &releaseHelper);
    std::vector<std::string> headersToVectorOfStrings();
    MOCKABLE_VIRTUAL void readFileToVectorOfStrings(const std::string &filename, std::vector<std::string> &lines);
    MOCKABLE_VIRTUAL std::vector<char> readBinaryFile(const std::string &filename);
    MOCKABLE_VIRTUAL std::unique_ptr<char[]> loadDataFromFile(const std::string &filename, size_t &retSize);

    void dontSetupOutputs() { hasOutput = false; }
    bool outputEnabled() const {
        return hasOutput;
    }
    bool hasHeaders() const {
        return headers.size() > 0;
    }
    const std::vector<Source> &getHeaders() const {
        return headers;
    }

    bool isVerbose() const {
        return verbose;
    }

    void setVerbose(bool verbose) {
        this->verbose = verbose;
    }

    MOCKABLE_VIRTUAL void saveOutput(const std::string &filename, const void *pData, const size_t &dataSize);

    MessagePrinter &getPrinterRef() { return messagePrinter; }
    void printf(const char *message) {
        messagePrinter.printf(message);
    }
    template <typename... Args>
    void printf(const char *format, Args... args) {
        messagePrinter.printf(format, std::forward<Args>(args)...);
    }
    template <typename EqComparableT>
    static auto findDuplicate(const EqComparableT &lhs) {
        return [&lhs](const auto &rhs) { return lhs == rhs; };
    }

    template <typename... Args>
    static auto getArgsWithoutDuplicate(const Args &...args) {
        std::vector<NEO::ConstStringRef> out{};
        for (const auto *acronyms : {std::addressof(args)...}) {
            for (const auto &acronym : *acronyms) {
                if (!std::any_of(out.begin(), out.end(), findDuplicate(acronym))) {
                    out.push_back(acronym);
                }
            }
        }
        return out;
    }

    template <typename... Args>
    static auto createStringForArgs(const Args &...args) {
        std::ostringstream os;
        for (const auto *acronyms : {std::addressof(args)...}) {
            for (const auto &acronym : *acronyms) {
                if (os.tellp())
                    os << ", ";
                os << acronym.str();
            }
        }
        return os.str();
    }

    std::unique_ptr<ProductConfigHelper> productConfigHelper;
};
