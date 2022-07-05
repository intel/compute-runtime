/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/decoder/helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/utilities/const_stringref.h"

#include "device_ids_configs.h"
#include "platforms.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

static constexpr auto *oclocStdoutLogName = "stdout.log";

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

struct DeviceProduct {
    unsigned short deviceId;
    std::string product;
};

struct DeviceMapping {
    const NEO::HardwareInfo *hwInfo = nullptr;
    const std::vector<unsigned short> *deviceIds = nullptr;
    AOT::FAMILY family = AOT::UNKNOWN_FAMILY;
    AOT::RELEASE release = AOT::UNKNOWN_RELEASE;
    AheadOfTimeConfig aotConfig = {0};
    std::vector<NEO::ConstStringRef> acronyms{};

    bool operator==(const DeviceMapping &rhs) {
        return aotConfig.ProductConfig == rhs.aotConfig.ProductConfig && family == rhs.family && release == rhs.release;
    }
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
    MessagePrinter messagePrinter;
    const std::vector<DeviceProduct> deviceProductTable;
    std::vector<DeviceMapping> deviceMap;
    void moveOutputs();
    Source *findSourceFile(const std::string &filename);
    bool sourceFileExists(const std::string &filename) const;

    inline void addOutput(const std::string &filename, const void *data, const size_t &size) {
        outputs.push_back(new Output(filename, data, size));
    }

    static bool compareConfigs(DeviceMapping deviceMap0, DeviceMapping deviceMap1) {
        return deviceMap0.aotConfig.ProductConfig < deviceMap1.aotConfig.ProductConfig;
    }

    template <typename EqComparableT>
    auto findFamily(const EqComparableT &lhs) {
        return [&lhs](const auto &rhs) { return lhs == rhs.family; };
    }

    template <typename EqComparableT>
    auto findRelease(const EqComparableT &lhs) {
        return [&lhs](const auto &rhs) { return lhs == rhs.release; };
    }

    template <typename EqComparableT>
    auto findProductConfig(const EqComparableT &lhs) {
        return [&lhs](const auto &rhs) { return lhs == rhs.aotConfig.ProductConfig; };
    }

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
    enum CONFIG_STATUS {
        MISMATCHED_VALUE = -1,
    };
    MOCKABLE_VIRTUAL bool fileExists(const std::string &filename) const;
    int parseProductConfigFromString(const std::string &device, size_t begin, size_t end);
    bool getHwInfoForProductConfig(uint32_t config, NEO::HardwareInfo &hwInfo, uint64_t hwInfoConfig);
    std::vector<DeviceMapping> &getAllSupportedDeviceConfigs();
    std::vector<NEO::ConstStringRef> getEnabledProductAcronyms();
    std::vector<NEO::ConstStringRef> getEnabledReleasesAcronyms();
    std::vector<NEO::ConstStringRef> getEnabledFamiliesAcronyms();
    std::vector<NEO::ConstStringRef> getDeprecatedAcronyms();
    std::vector<NEO::ConstStringRef> getAllSupportedProductAcronyms();
    std::string getAllSupportedAcronyms();
    AOT::PRODUCT_CONFIG getProductConfigForVersionValue(const std::string &device);
    bool setAcronymForDeviceId(std::string &device);
    std::vector<std::string> headersToVectorOfStrings();
    MOCKABLE_VIRTUAL void readFileToVectorOfStrings(const std::string &filename, std::vector<std::string> &lines);
    MOCKABLE_VIRTUAL std::vector<char> readBinaryFile(const std::string &filename);
    MOCKABLE_VIRTUAL std::unique_ptr<char[]> loadDataFromFile(const std::string &filename, size_t &retSize);

    bool outputEnabled() const {
        return hasOutput;
    }
    bool hasHeaders() const {
        return headers.size() > 0;
    }
    const std::vector<Source> &getHeaders() const {
        return headers;
    }

    MOCKABLE_VIRTUAL void saveOutput(const std::string &filename, const void *pData, const size_t &dataSize);
    void saveOutput(const std::string &filename, const std::ostream &stream);

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
    static auto getArgsWithoutDuplicate(Args... args) {
        std::vector<NEO::ConstStringRef> out{};
        for (const auto &acronyms : {args...}) {
            for (const auto &acronym : acronyms) {
                if (!std::any_of(out.begin(), out.end(), findDuplicate(acronym))) {
                    out.push_back(acronym);
                }
            }
        }
        return out;
    }

    template <typename... Args>
    static auto createStringForArgs(Args... args) {
        std::ostringstream os;
        for (const auto &acronyms : {args...}) {
            for (const auto &acronym : acronyms) {
                if (os.tellp())
                    os << ", ";
                os << acronym.str();
            }
        }
        return os.str();
    }

    bool isRelease(const std::string &device);
    bool isFamily(const std::string &device);
    bool isProductConfig(const std::string &device);
    bool areQuotesRequired(const std::string_view &argName);

    std::string returnProductNameForDevice(unsigned short deviceId);
};
