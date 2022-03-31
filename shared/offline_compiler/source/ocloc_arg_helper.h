/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/decoder/helper.h"
#include "shared/source/helpers/hw_info.h"

#include "device_ids_configs.h"
#include "hw_cmds.h"
#include "platforms.h"

#include <cctype>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#pragma once

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
    PRODUCT_CONFIG config;
    const NEO::HardwareInfo *hwInfo;
    const std::vector<unsigned short> *deviceIds;
    void (*setupFeatureAndWorkaroundTable)(NEO::HardwareInfo *hwInfo);
    unsigned int revId;

    bool operator==(const DeviceMapping &rhs) {
        return config == rhs.config && hwInfo == rhs.hwInfo && setupFeatureAndWorkaroundTable == rhs.setupFeatureAndWorkaroundTable && revId == rhs.revId;
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
    DeviceMapping deviceForFatbinary;
    std::map<std::string, unsigned int> genIGFXMap;
    bool fatBinary = false;
    void moveOutputs();
    Source *findSourceFile(const std::string &filename);
    bool sourceFileExists(const std::string &filename) const;

    inline void addOutput(const std::string &filename, const void *data, const size_t &size) {
        outputs.push_back(new Output(filename, data, size));
    }

    static bool compareConfigs(DeviceMapping deviceMap0, DeviceMapping deviceMap1) {
        return deviceMap0.config < deviceMap1.config;
    }

    static bool isDuplicateConfig(DeviceMapping deviceMap0, DeviceMapping deviceMap1) {
        return deviceMap0.config == deviceMap1.config;
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
    const std::string parseProductConfigFromValue(PRODUCT_CONFIG config);
    bool getHwInfoForProductConfig(uint32_t config, NEO::HardwareInfo &hwInfo);
    void getProductConfigsForGfxCoreFamily(GFXCORE_FAMILY core, std::vector<DeviceMapping> &out);
    void setDeviceInfoForFatbinaryTarget(const DeviceMapping &device);
    void setHwInfoForFatbinaryTarget(NEO::HardwareInfo &hwInfo);
    std::vector<PRODUCT_CONFIG> getAllSupportedProductConfigs();
    std::vector<DeviceMapping> &getAllSupportedDeviceConfigs();
    std::vector<uint32_t> getMajorMinorRevision(const std::string &device);
    uint32_t getProductConfig(std::vector<uint32_t> &numeration);
    uint32_t getMaskForConfig(std::vector<uint32_t> &numeration);
    PRODUCT_CONFIG findConfigMatch(const std::string &device, bool firstAppearance);
    void insertGenNames(GFXCORE_FAMILY family);
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

    void setFatbinary(bool isFatBinary) {
        this->fatBinary = isFatBinary;
    }

    bool isFatbinary() {
        return fatBinary;
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

    std::string returnProductNameForDevice(unsigned short deviceId);
    bool isGen(const std::string &device);
    unsigned int returnIGFXforGen(const std::string &device);
    bool areQuotesRequired(const std::string_view &argName);
};
