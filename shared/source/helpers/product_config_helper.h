/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/utilities/const_stringref.h"

#include "neo_igfxfmid.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace AOT {
enum PRODUCT_CONFIG : uint32_t; // NOLINT(readability-identifier-naming)
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
    std::vector<NEO::ConstStringRef> deviceAcronyms{};
    std::vector<NEO::ConstStringRef> rtlIdAcronyms{};

    bool operator==(const DeviceAotInfo &rhs) const {
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
    static void adjustProductConfig(uint32_t &productConfig);
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
    static NEO::ConstStringRef getAcronymFromAFamily(AOT::FAMILY family);
    static NEO::ConstStringRef getAcronymFromARelease(AOT::RELEASE release);
    static uint32_t getProductConfigFromVersionValue(const std::string &device);
    static AOT::PRODUCT_CONFIG getProductConfigFromAcronym(const std::string &device);

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
        return [&lhs](const auto &rhs) { return std::any_of(rhs.deviceAcronyms.begin(), rhs.deviceAcronyms.end(), findAcronymWithoutDash(lhs)) ||
                                                std::any_of(rhs.rtlIdAcronyms.begin(), rhs.rtlIdAcronyms.end(), findAcronymWithoutDash(lhs)); };
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

    template <typename EqComparableT>
    static auto findDeviceAcronymForRelease(const EqComparableT &lhs) {
        return [&lhs](const auto &rhs) { return lhs == rhs.release && !rhs.deviceAcronyms.empty(); };
    }

    void initialize();
    void adjustClosedRangeDeviceLegacyAcronyms(std::string &rangeFromStr, std::string &rangeToStr);
    bool isSupportedFamily(uint32_t family) const;
    bool isSupportedRelease(uint32_t release) const;
    bool isSupportedProductConfig(uint32_t config) const;
    bool getDeviceAotInfoForProductConfig(uint32_t config, DeviceAotInfo &out) const;

    std::vector<DeviceAotInfo> &getDeviceAotInfo();
    std::vector<NEO::ConstStringRef> getRepresentativeProductAcronyms();
    std::vector<NEO::ConstStringRef> getReleasesAcronyms();
    std::vector<NEO::ConstStringRef> getFamiliesAcronyms();
    std::vector<NEO::ConstStringRef> getDeprecatedAcronyms();
    std::vector<NEO::ConstStringRef> getAllProductAcronyms();
    AOT::FAMILY getFamilyFromDeviceName(const std::string &device) const;
    AOT::PRODUCT_CONFIG getProductConfigFromDeviceName(const std::string &device) const;
    PRODUCT_FAMILY getProductFamilyFromDeviceName(const std::string &device) const;
    AOT::RELEASE getReleaseFromDeviceName(const std::string &device) const;
    const std::string getAcronymForProductConfig(uint32_t config) const;

  protected:
    std::vector<DeviceAotInfo> deviceAotInfo;
};
