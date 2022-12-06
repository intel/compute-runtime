/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/utilities/const_stringref.h"

#include "igfxfmid.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace AOT {
enum PRODUCT_CONFIG : uint32_t;
enum RELEASE : uint32_t;
enum FAMILY : uint32_t;
} // namespace AOT

namespace NEO {
struct HardwareInfo;
} // namespace NEO

struct DeviceAotInfo {
    NEO::HardwareIpVersion aotConfig{};
    const NEO::HardwareInfo *hwInfo = nullptr;
    const std::vector<unsigned short> *deviceIds = nullptr;
    AOT::FAMILY family = {};
    AOT::RELEASE release = {};
    std::vector<NEO::ConstStringRef> acronyms{};

    bool operator==(const DeviceAotInfo &rhs) {
        return aotConfig.value == rhs.aotConfig.value && family == rhs.family && release == rhs.release && hwInfo == rhs.hwInfo;
    }
};

struct ProductConfigHelper {
  public:
    ProductConfigHelper();
    enum ConfigStatus {
        MismatchedValue = -1,
    };
    static void adjustDeviceName(std::string &device);
    static std::string parseMajorMinorValue(NEO::HardwareIpVersion config);
    static std::string parseMajorMinorRevisionValue(NEO::HardwareIpVersion config);
    static int parseProductConfigFromString(const std::string &device, size_t begin, size_t end);
    inline static std::string parseMajorMinorRevisionValue(AOT::PRODUCT_CONFIG config) {
        std::stringstream stringConfig;
        NEO::HardwareIpVersion aotConfig = {0};
        aotConfig.value = config;
        return parseMajorMinorRevisionValue(aotConfig);
    }

    static std::vector<NEO::ConstStringRef> getDeviceAcronyms();
    static NEO::ConstStringRef getAcronymForAFamily(AOT::FAMILY family);
    static AOT::PRODUCT_CONFIG getProductConfigForVersionValue(const std::string &device);
    static AOT::PRODUCT_CONFIG getProductConfigForAcronym(const std::string &device);
    static AOT::RELEASE getReleaseForAcronym(const std::string &device);
    static AOT::FAMILY getFamilyForAcronym(const std::string &device);

    static bool compareConfigs(DeviceAotInfo deviceAotInfo0, DeviceAotInfo deviceAotInfo1);

    template <typename EqComparableT>
    static auto findMapAcronymWithoutDash(const EqComparableT &lhs) {
        return [&lhs](const auto &rhs) {
            NEO::ConstStringRef ptrStr(rhs.first);
            return lhs == ptrStr || ptrStr.isEqualWithoutSeparator('-', lhs.c_str()); };
    }

    template <typename EqComparableT>
    static auto findAcronymWithoutDash(const EqComparableT &lhs) {
        return [&lhs](const auto &rhs) { return lhs == rhs || rhs.isEqualWithoutSeparator('-', lhs.c_str()); };
    }

    template <typename EqComparableT>
    static auto findAcronym(const EqComparableT &lhs) {
        return [&lhs](const auto &rhs) { return std::any_of(rhs.acronyms.begin(), rhs.acronyms.end(), findAcronymWithoutDash(lhs)); };
    }

    template <typename EqComparableT>
    static auto findFamily(const EqComparableT &lhs) {
        return [&lhs](const auto &rhs) { return lhs == rhs.family; };
    }

    template <typename EqComparableT>
    static auto findRelease(const EqComparableT &lhs) {
        return [&lhs](const auto &rhs) { return lhs == rhs.release; };
    }

    template <typename EqComparableT>
    static auto findProductConfig(const EqComparableT &lhs) {
        return [&lhs](const auto &rhs) { return lhs == rhs.aotConfig.value; };
    }

    void initialize();
    bool isFamily(const std::string &device);
    bool isRelease(const std::string &device);
    bool isProductConfig(const std::string &device);
    bool getDeviceAotInfoForProductConfig(AOT::PRODUCT_CONFIG config, DeviceAotInfo &out) const;

    std::vector<DeviceAotInfo> &getDeviceAotInfo();
    std::vector<NEO::ConstStringRef> getRepresentativeProductAcronyms();
    std::vector<NEO::ConstStringRef> getReleasesAcronyms();
    std::vector<NEO::ConstStringRef> getFamiliesAcronyms();
    std::vector<NEO::ConstStringRef> getDeprecatedAcronyms();
    std::vector<NEO::ConstStringRef> getAllProductAcronyms();
    PRODUCT_FAMILY getProductFamilyForAcronym(const std::string &device) const;

  protected:
    std::vector<DeviceAotInfo> deviceAotInfo;
};
