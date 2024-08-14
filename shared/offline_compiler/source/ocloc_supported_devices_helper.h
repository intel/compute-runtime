/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/helpers/product_config_helper.h"

#include <map>
#include <string>
#include <vector>

namespace Ocloc {

namespace SupportedDevicesYamlConstants {
constexpr std::string_view deviceIpVersions = "device_ip_versions";
constexpr std::string_view ipToDevRevId = "ip_to_dev_rev_id";
constexpr std::string_view acronym = "acronym";
constexpr std::string_view familyGroups = "family_groups";
constexpr std::string_view releaseGroups = "release_groups";

// Fields in IpToDevRevId section
constexpr std::string_view ip = "ip";
constexpr std::string_view revisionId = "revision_id";
constexpr std::string_view deviceId = "device_id";
} // namespace SupportedDevicesYamlConstants

enum class SupportedDevicesMode {
    merge,
    concat,
    unknown
};

class SupportedDevicesHelper {
  public:
    struct DeviceInfo {
        uint32_t deviceId;
        uint32_t revisionId;
        uint32_t ipVersion;
    };

    struct SupportedDevicesData {
        std::vector<uint32_t> deviceIpVersions;
        std::vector<DeviceInfo> deviceInfos;
        std::vector<std::pair<std::string, uint32_t>> acronyms;
        std::vector<std::pair<std::string, std::vector<uint32_t>>> familyGroups;
        std::vector<std::pair<std::string, std::vector<uint32_t>>> releaseGroups;
    };

  public:
    SupportedDevicesHelper(SupportedDevicesMode mode, ProductConfigHelper *productConfigHelper)
        : mode(mode), productConfigHelper(productConfigHelper) {}

    std::string getCurrentOclocOutputFilename() const;
    SupportedDevicesData collectSupportedDevicesData(const std::vector<DeviceAotInfo> &enabledDevices) const;

    std::string serialize(std::string_view oclocName, const SupportedDevicesData &data) const;
    std::map<std::string, SupportedDevicesData> deserialize(std::string_view yamlString) const;

    std::string mergeAndSerializeWithFormerData(const SupportedDevicesData &currentData) const;
    std::string concatAndSerializeWithFormerData(const SupportedDevicesData &currentData) const;

    MOCKABLE_VIRTUAL std::string getDataFromFormerOcloc() const;

  protected:
    MOCKABLE_VIRTUAL std::string getCurrentOclocName() const;
    std::string getFormerOclocName() const;
    std::string extractOclocName(std::string_view oclocLibFileName) const;
    SupportedDevicesData mergeOclocData(const std::map<std::string, SupportedDevicesData> &nameDataMap) const;
    std::string getOutputFilenameSuffix([[maybe_unused]] SupportedDevicesMode mode) const;

  private:
    SupportedDevicesMode mode;
    ProductConfigHelper *productConfigHelper;
    static constexpr std::string_view fileExtension = ".yaml";
};

inline SupportedDevicesMode parseSupportedDevicesMode(std::string_view modeStr) {
    if (modeStr == "-merge") {
        return SupportedDevicesMode::merge;
    } else if (modeStr == "-concat") {
        return SupportedDevicesMode::concat;
    } else {
        return SupportedDevicesMode::unknown;
    }
}

inline std::string toStr(SupportedDevicesMode mode) {
    switch (mode) {
    case SupportedDevicesMode::concat:
        return "concat";
    case SupportedDevicesMode::merge:
        return "merge";
    default:
        return "unknown";
    }
}
} // namespace Ocloc
