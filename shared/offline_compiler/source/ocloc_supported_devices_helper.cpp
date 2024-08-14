/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_supported_devices_helper.h"

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_interface.h"
#include "shared/source/device_binary_format/yaml/yaml_parser.h"
#include "shared/source/helpers/product_config_helper.h"

#include <cstring>
#include <memory>
#include <set>
#include <sstream>
#include <vector>

namespace Ocloc {
using namespace NEO;

SupportedDevicesHelper::SupportedDevicesData SupportedDevicesHelper::collectSupportedDevicesData(
    const std::vector<DeviceAotInfo> &enabledDevices) const {

    SupportedDevicesData data;

    // Populate IP Versions, Device Infos, Acronyms
    for (const auto &device : enabledDevices) {
        data.deviceIpVersions.push_back(device.aotConfig.value);

        for (const auto &deviceId : *device.deviceIds) {
            data.deviceInfos.push_back({deviceId, device.aotConfig.revision, device.aotConfig.value});
        }

        for (const auto &acronym : device.deviceAcronyms) {
            data.acronyms.push_back({acronym.data(), device.aotConfig.value});
        }
    }

    // Populate Family groups
    std::map<AOT::FAMILY, std::vector<uint32_t>> groupedDevices;
    for (const auto &device : enabledDevices) {
        groupedDevices[device.family].push_back(device.aotConfig.value);
    }
    for (const auto &entry : groupedDevices) {
        data.familyGroups.push_back({productConfigHelper->getAcronymFromAFamily(entry.first).data(), entry.second});
    }

    // Populate Release groups
    std::map<AOT::RELEASE, std::vector<uint32_t>> groupedReleases;
    for (const auto &device : enabledDevices) {
        groupedReleases[device.release].push_back(device.aotConfig.value);
    }
    for (const auto &entry : groupedReleases) {
        auto name = productConfigHelper->getAcronymFromARelease(entry.first);
        if (!name.empty()) {
            data.releaseGroups.push_back({name.data(), entry.second});
        }
    }

    return data;
}

std::string SupportedDevicesHelper::serialize(std::string_view oclocName, const SupportedDevicesData &data) const {
    std::ostringstream oss;
    oss << oclocName << ":\n";

    // DeviceIpVersions
    oss << "  " << SupportedDevicesYamlConstants::deviceIpVersions << ":\n";
    for (const auto &ipVersion : data.deviceIpVersions) {
        oss << "    - 0x" << std::hex << ipVersion << "\n";
    }

    // IpToDevRevId
    oss << "  " << SupportedDevicesYamlConstants::ipToDevRevId << ":\n";
    for (const auto &device : data.deviceInfos) {
        oss << "    - " << SupportedDevicesYamlConstants::ip << ": 0x" << std::hex << device.ipVersion << "\n";
        oss << "      " << SupportedDevicesYamlConstants::revisionId << ": " << std::dec << device.revisionId << "\n";
        oss << "      " << SupportedDevicesYamlConstants::deviceId << ": 0x" << std::hex << device.deviceId << "\n";
    }

    // Acronym
    oss << "  " << SupportedDevicesYamlConstants::acronym << ":\n";
    for (const auto &[acronym, ipVersion] : data.acronyms) {
        oss << "    " << acronym << ": 0x" << std::hex << ipVersion << "\n";
    }

    // FamilyGroups
    oss << "  " << SupportedDevicesYamlConstants::familyGroups << ":\n";
    for (const auto &[family, ipVersions] : data.familyGroups) {
        oss << "    " << family << ": [";
        for (size_t i = 0; i < ipVersions.size(); ++i) {
            oss << "0x" << std::hex << ipVersions[i];
            if (i < ipVersions.size() - 1)
                oss << ", ";
        }
        oss << "]\n";
    }

    // ReleaseGroups
    oss << "  " << SupportedDevicesYamlConstants::releaseGroups << ":\n";
    for (const auto &[release, ipVersions] : data.releaseGroups) {
        oss << "    " << release << ": [";
        for (size_t i = 0; i < ipVersions.size(); ++i) {
            oss << "0x" << std::hex << ipVersions[i];
            if (i < ipVersions.size() - 1)
                oss << ", ";
        }
        oss << "]\n";
    }

    return oss.str();
}

std::map<std::string, SupportedDevicesHelper::SupportedDevicesData> SupportedDevicesHelper::deserialize(std::string_view yamlString) const {
    std::map<std::string, SupportedDevicesData> result;
    NEO::Yaml::YamlParser parser;
    std::string errReason, warning;

    if (!parser.parse(yamlString.data(), errReason, warning)) {
        DEBUG_BREAK_IF(true);
        return result;
    }

    const auto *root = parser.getRoot();
    for (const auto &oclocVersionNode : parser.createChildrenRange(*root)) {
        std::string oclocVersion = parser.readKey(oclocVersionNode).str();
        SupportedDevicesData &data = result[oclocVersion];

        const auto *ipVersionsNode = parser.getChild(oclocVersionNode, SupportedDevicesYamlConstants::deviceIpVersions.data());
        if (ipVersionsNode) {
            for (const auto &ipNode : parser.createChildrenRange(*ipVersionsNode)) {
                uint32_t ipVersion;
                if (parser.readValueChecked(ipNode, ipVersion)) {
                    data.deviceIpVersions.push_back(ipVersion);
                }
            }
        }

        const auto *deviceIdsNode = parser.getChild(oclocVersionNode, SupportedDevicesYamlConstants::ipToDevRevId.data());
        if (deviceIdsNode) {
            for (const auto &deviceNode : parser.createChildrenRange(*deviceIdsNode)) {
                DeviceInfo info;
                const auto *deviceIdNode = parser.getChild(deviceNode, SupportedDevicesYamlConstants::deviceId.data());
                const auto *revisionIdNode = parser.getChild(deviceNode, SupportedDevicesYamlConstants::revisionId.data());
                const auto *ipNode = parser.getChild(deviceNode, SupportedDevicesYamlConstants::ip.data());
                if (deviceIdNode && revisionIdNode && ipNode) {
                    parser.readValueChecked(*deviceIdNode, info.deviceId);
                    parser.readValueChecked(*revisionIdNode, info.revisionId);
                    parser.readValueChecked(*ipNode, info.ipVersion);
                    data.deviceInfos.push_back(info);
                }
            }
        }

        const auto *acronymNode = parser.getChild(oclocVersionNode, SupportedDevicesYamlConstants::acronym.data());
        if (acronymNode) {
            for (const auto &acrNode : parser.createChildrenRange(*acronymNode)) {
                std::string acronym = parser.readKey(acrNode).str();
                uint32_t ipVersion;
                if (parser.readValueChecked(acrNode, ipVersion)) {
                    data.acronyms.push_back({acronym, ipVersion});
                }
            }
        }

        const auto *familyGroupsNode = parser.getChild(oclocVersionNode, SupportedDevicesYamlConstants::familyGroups.data());
        if (familyGroupsNode) {
            for (const auto &familyNode : parser.createChildrenRange(*familyGroupsNode)) {
                std::string family = parser.readKey(familyNode).str();
                std::vector<uint32_t> ipVersions;
                for (const auto &ipVersionNode : parser.createChildrenRange(familyNode)) {
                    uint32_t ipVersion;
                    if (parser.readValueChecked(ipVersionNode, ipVersion)) {
                        ipVersions.push_back(ipVersion);
                    }
                }
                if (!ipVersions.empty()) {
                    data.familyGroups.push_back({family, ipVersions});
                }
            }
        }

        const auto *releaseGroupsNode = parser.getChild(oclocVersionNode, SupportedDevicesYamlConstants::releaseGroups.data());
        if (releaseGroupsNode) {
            for (const auto &releaseNode : parser.createChildrenRange(*releaseGroupsNode)) {
                std::string release = parser.readKey(releaseNode).str();
                std::vector<uint32_t> ipVersions;
                for (const auto &ipVersionNode : parser.createChildrenRange(releaseNode)) {
                    uint32_t ipVersion;
                    if (parser.readValueChecked(ipVersionNode, ipVersion)) {
                        ipVersions.push_back(ipVersion);
                    }
                }
                if (!ipVersions.empty()) {
                    data.releaseGroups.push_back({release, ipVersions});
                }
            }
        }
    }

    return result;
}

std::string SupportedDevicesHelper::mergeAndSerializeWithFormerData(const SupportedDevicesData &currentData) const {
    std::string formerSupportedDevices = getDataFromFormerOcloc();

    if (formerSupportedDevices.empty()) {
        return serialize(getCurrentOclocName(), currentData);
    }

    auto formerDataDeserialized = deserialize(formerSupportedDevices);
    formerDataDeserialized[getCurrentOclocName()] = currentData;

    auto mergedData = mergeOclocData(formerDataDeserialized);
    return serialize("ocloc", mergedData);
}

std::string SupportedDevicesHelper::concatAndSerializeWithFormerData(const SupportedDevicesData &currentData) const {
    std::string output = serialize(getCurrentOclocName(), currentData);
    std::string formerSupportedDevices = getDataFromFormerOcloc();
    if (!formerSupportedDevices.empty()) {
        output += "\n" + formerSupportedDevices;
    }
    return output;
}

SupportedDevicesHelper::SupportedDevicesData SupportedDevicesHelper::mergeOclocData(const std::map<std::string, SupportedDevicesData> &nameDataMap) const {
    struct DeviceInfoComparator {
        bool operator()(const DeviceInfo &lhs, const DeviceInfo &rhs) const {
            return std::tie(lhs.deviceId, lhs.revisionId, lhs.ipVersion) <
                   std::tie(rhs.deviceId, rhs.revisionId, rhs.ipVersion);
        }
    };

    SupportedDevicesData mergedData;
    std::set<uint32_t> uniqueIpVersions;
    std::set<DeviceInfo, DeviceInfoComparator> uniqueDeviceInfos;
    std::set<std::pair<std::string, uint32_t>> uniqueAcronyms;
    std::map<std::string, std::set<uint32_t>> uniqueFamilyGroups;
    std::map<std::string, std::set<uint32_t>> uniqueReleaseGroups;

    for (const auto &[name, data] : nameDataMap) {
        uniqueIpVersions.insert(data.deviceIpVersions.begin(), data.deviceIpVersions.end());
        for (const auto &deviceInfo : data.deviceInfos) {
            uniqueDeviceInfos.insert(deviceInfo);
        }
        uniqueAcronyms.insert(data.acronyms.begin(), data.acronyms.end());
        for (const auto &[family, ipVersions] : data.familyGroups) {
            uniqueFamilyGroups[family].insert(ipVersions.begin(), ipVersions.end());
        }
        for (const auto &[release, ipVersions] : data.releaseGroups) {
            uniqueReleaseGroups[release].insert(ipVersions.begin(), ipVersions.end());
        }
    }

    // Sort DeviceIpVersions (ascending)
    mergedData.deviceIpVersions = std::vector<uint32_t>(uniqueIpVersions.begin(), uniqueIpVersions.end());
    std::sort(mergedData.deviceIpVersions.begin(), mergedData.deviceIpVersions.end());

    // Sort IpToDevRevId (ascending by ipVersion)
    mergedData.deviceInfos = std::vector<DeviceInfo>(uniqueDeviceInfos.begin(), uniqueDeviceInfos.end());
    std::sort(mergedData.deviceInfos.begin(), mergedData.deviceInfos.end(),
              [](const DeviceInfo &a, const DeviceInfo &b) { return a.ipVersion < b.ipVersion; });

    // Sort Acronyms (ascending by ipVersion)
    mergedData.acronyms = std::vector<std::pair<std::string, uint32_t>>(uniqueAcronyms.begin(), uniqueAcronyms.end());
    std::sort(mergedData.acronyms.begin(), mergedData.acronyms.end(),
              [](const auto &a, const auto &b) { return a.second < b.second; });

    // Sort FamilyGroups (alphabetically by group name)
    for (const auto &[family, ipVersions] : uniqueFamilyGroups) {
        mergedData.familyGroups.push_back({family, std::vector<uint32_t>(ipVersions.begin(), ipVersions.end())});
    }
    std::sort(mergedData.familyGroups.begin(), mergedData.familyGroups.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });

    // Sort ReleaseGroups (alphabetically by group name)
    for (const auto &[release, ipVersions] : uniqueReleaseGroups) {
        mergedData.releaseGroups.push_back({release, std::vector<uint32_t>(ipVersions.begin(), ipVersions.end())});
    }
    std::sort(mergedData.releaseGroups.begin(), mergedData.releaseGroups.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });

    // Sort IP versions within each FAMILY and RELEASE group
    for (auto &[_, ipVersions] : mergedData.familyGroups) {
        std::sort(ipVersions.begin(), ipVersions.end());
    }
    for (auto &[_, ipVersions] : mergedData.releaseGroups) {
        std::sort(ipVersions.begin(), ipVersions.end());
    }

    return mergedData;
}

std::string SupportedDevicesHelper::getFormerOclocName() const {
    return extractOclocName(getOclocFormerLibName());
}
} // namespace Ocloc
