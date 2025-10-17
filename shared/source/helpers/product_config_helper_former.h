/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_interface.h"
#include "shared/offline_compiler/source/ocloc_supported_devices_helper.h"
#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/utilities/const_stringref.h"

#include "neo_igfxfmid.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

struct ProductConfigHelper;
struct FormerProductConfigHelper {
  public:
    enum class Position { firstItem,
                          lastItem };

    FormerProductConfigHelper();

    Ocloc::SupportedDevicesHelper::SupportedDevicesData &getData();

    template <typename T>
    bool isSupportedTarget(const std::string &target) const;
    bool isSupportedProductConfig(uint32_t config) const;
    std::vector<NEO::ConstStringRef> getProductAcronymsFromFamilyGroup(const std::string &groupName) const;
    std::vector<NEO::ConstStringRef> getProductAcronymsFromReleaseGroup(const std::string &groupName) const;
    AOT::PRODUCT_CONFIG getProductConfigFromDeviceName(const std::string &device);
    AOT::PRODUCT_CONFIG getProductConfigFromAcronym(const std::string &device);
    AOT::PRODUCT_CONFIG getProductConfigFromDeviceId(const uint32_t &deviceId);
    AOT::PRODUCT_CONFIG getFirstProductConfig();
    bool isFamilyName(const std::string &family);
    bool isReleaseName(const std::string &release);
    std::string getFamilyName(Position pos);
    std::string getReleaseName(Position pos);
    AOT::PRODUCT_CONFIG getProductConfigFromFamilyName(const std::string &family, Position pos);
    AOT::PRODUCT_CONFIG getProductConfigFromReleaseName(const std::string &release, Position pos);
    std::vector<NEO::ConstStringRef> getRepresentativeProductAcronyms();

  protected:
    template <typename GroupT>
    std::vector<NEO::ConstStringRef> getProductAcronymsFromGroup(const std::string &groupName, const GroupT &groups) const;
    Ocloc::SupportedDevicesHelper helper;
    Ocloc::SupportedDevicesHelper::SupportedDevicesData data;
};

template <typename GroupT>
std::vector<NEO::ConstStringRef> FormerProductConfigHelper::getProductAcronymsFromGroup(const std::string &groupName, const GroupT &groups) const {
    std::vector<NEO::ConstStringRef> enabledAcronyms{};
    for (const auto &[name, ipVersions] : groups) {
        if (name == groupName) {
            for (const auto &ipVersion : ipVersions) {
                auto it = std::find_if(data.acronyms.begin(), data.acronyms.end(),
                                       [&ipVersion](const std::pair<std::string, uint32_t> &pair) {
                                           return pair.second == ipVersion;
                                       });
                if (it != data.acronyms.end()) {
                    enabledAcronyms.push_back(it->first);
                }
            }
        }
    }
    return enabledAcronyms;
}
