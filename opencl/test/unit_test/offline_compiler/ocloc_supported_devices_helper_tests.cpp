/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_supported_devices_helper.h"
#include "shared/source/device_binary_format/yaml/yaml_parser.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_ocloc_supported_devices_helper.h"

#include "gtest/gtest.h"
#include "mock/mock_argument_helper.h"

#include <set>
#include <string>
#include <vector>

namespace NEO {

struct SupportedDevicesHelperTest : public ::testing::Test {
    struct MockDeviceData {
        uint32_t ipVersion;
        uint32_t revision;
        std::vector<uint16_t> deviceIds;
        std::vector<ConstStringRef> acronyms;
        AOT::FAMILY family;
        AOT::RELEASE release;
    };

    void initMockDevices() {
        auto deviceAotInfo = argHelper->productConfigHelper->getDeviceAotInfo();

        if (deviceAotInfo.size() < 2) {
            GTEST_SKIP();
        }

        std::set<AOT::FAMILY> families;
        std::set<AOT::RELEASE> releases;

        for (const auto &device : deviceAotInfo) {
            families.insert(device.family);
            releases.insert(device.release);
        }

        if (families.size() < 2 || releases.size() < 2) {
            GTEST_SKIP();
        }

        auto familyIt = families.begin();
        family1 = *familyIt;
        family2 = *(++familyIt);

        auto releaseIt = releases.begin();
        release1 = *releaseIt;
        release2 = *(++releaseIt);

        if (argHelper->productConfigHelper->getAcronymFromAFamily(family1).empty() ||
            argHelper->productConfigHelper->getAcronymFromAFamily(family2).empty() ||
            argHelper->productConfigHelper->getAcronymFromARelease(release1).empty() ||
            argHelper->productConfigHelper->getAcronymFromARelease(release2).empty()) {
            GTEST_SKIP();
        }

        const auto revisionBits = 0x3F;

        mockDevices = {
            {0x1000001, 0x1000001 & revisionBits, {0x1111, 0x1100}, {"aaa", "bbb"}, family1, release1},
            {0x2000002, 0x2000002 & revisionBits, {0x2222, 0x2200}, {"ccc", "ddd"}, family2, release2}};
    }

    void prepareMockDeviceAotInfo(std::vector<DeviceAotInfo> &dest) {
        for (const auto &mockDevice : mockDevices) {
            DeviceAotInfo enabledDevice;
            enabledDevice.aotConfig.value = mockDevice.ipVersion;
            enabledDevice.aotConfig.revision = mockDevice.revision;
            enabledDevice.deviceIds = new std::vector<unsigned short>(mockDevice.deviceIds);
            enabledDevice.deviceAcronyms = mockDevice.acronyms;
            enabledDevice.family = mockDevice.family;
            enabledDevice.release = mockDevice.release;
            dest.push_back(enabledDevice);
        }
    }
    void SetUp() override {
        initMockDevices();
        prepareMockDeviceAotInfo(enabledDevices);
    }

    void TearDown() override {
        for (auto &device : enabledDevices) {
            delete device.deviceIds;
        }
    }

    std::map<std::string, std::string> filesMap;
    std::unique_ptr<MockOclocArgHelper> argHelper = std::make_unique<MockOclocArgHelper>(filesMap);
    std::vector<MockDeviceData> mockDevices;
    std::vector<DeviceAotInfo> enabledDevices;

    AOT::FAMILY family1;
    AOT::FAMILY family2;

    AOT::RELEASE release1;
    AOT::RELEASE release2;
};

TEST_F(SupportedDevicesHelperTest, WhenCollectingSupportedDevicesDataThenAllFieldsAreCorrectlyPopulated) {
    SupportedDevicesMode mode = SupportedDevicesMode::concat;
    SupportedDevicesHelper supportedDevicesHelper(mode, argHelper->productConfigHelper.get());

    auto supportedDevicesData = supportedDevicesHelper.collectSupportedDevicesData(enabledDevices);

    // Verify deviceIpVersions
    EXPECT_EQ(supportedDevicesData.deviceIpVersions.size(), mockDevices.size());
    for (size_t i = 0; i < mockDevices.size(); ++i) {
        EXPECT_EQ(supportedDevicesData.deviceIpVersions[i], mockDevices[i].ipVersion);
    }

    // Verify deviceInfos
    size_t expectedDeviceInfosSize = 0;
    for (const auto &mockDevice : mockDevices) {
        expectedDeviceInfosSize += mockDevice.deviceIds.size();
    }
    EXPECT_EQ(supportedDevicesData.deviceInfos.size(), expectedDeviceInfosSize);

    size_t deviceInfoIndex = 0;
    for (const auto &mockDevice : mockDevices) {
        for (const auto &deviceId : mockDevice.deviceIds) {
            EXPECT_EQ(supportedDevicesData.deviceInfos[deviceInfoIndex].deviceId, deviceId);
            EXPECT_EQ(supportedDevicesData.deviceInfos[deviceInfoIndex].revisionId, mockDevice.revision);
            EXPECT_EQ(supportedDevicesData.deviceInfos[deviceInfoIndex].ipVersion, mockDevice.ipVersion);
            ++deviceInfoIndex;
        }
    }

    // Verify acronyms
    size_t expectedAcronymsSize = 0;
    for (const auto &mockDevice : mockDevices) {
        expectedAcronymsSize += mockDevice.acronyms.size();
    }
    EXPECT_EQ(supportedDevicesData.acronyms.size(), expectedAcronymsSize);

    size_t acronymIndex = 0;
    for (const auto &mockDevice : mockDevices) {
        for (const auto &acronym : mockDevice.acronyms) {
            EXPECT_EQ(supportedDevicesData.acronyms[acronymIndex].first, acronym.data());
            EXPECT_EQ(supportedDevicesData.acronyms[acronymIndex].second, mockDevice.ipVersion);
            ++acronymIndex;
        }
    }

    // Verify familyGroups
    EXPECT_EQ(supportedDevicesData.familyGroups.size(), mockDevices.size());
    for (const auto &mockDevice : mockDevices) {
        auto findFamily = [&](const std::string &name) {
            return std::find_if(supportedDevicesData.familyGroups.begin(), supportedDevicesData.familyGroups.end(),
                                [&](const auto &group) { return group.first == name; });
        };
        auto familyName = argHelper->productConfigHelper->getAcronymFromAFamily(mockDevice.family);
        auto familyIt = findFamily(familyName.data());
        ASSERT_NE(familyIt, supportedDevicesData.familyGroups.end());
        EXPECT_TRUE(std::find(familyIt->second.begin(), familyIt->second.end(), mockDevice.ipVersion) != familyIt->second.end());
    }

    // Verify releaseGroups
    EXPECT_EQ(supportedDevicesData.releaseGroups.size(), mockDevices.size());
    for (const auto &mockDevice : mockDevices) {
        auto findRelease = [&](const std::string &name) {
            return std::find_if(supportedDevicesData.releaseGroups.begin(), supportedDevicesData.releaseGroups.end(),
                                [&](const auto &group) { return group.first == name; });
        };
        auto releaseName = argHelper->productConfigHelper->getAcronymFromARelease(mockDevice.release);
        auto releaseIt = findRelease(releaseName.data());
        ASSERT_NE(releaseIt, supportedDevicesData.releaseGroups.end());
        EXPECT_TRUE(std::find(releaseIt->second.begin(), releaseIt->second.end(), mockDevice.ipVersion) != releaseIt->second.end());
    }
}

TEST_F(SupportedDevicesHelperTest, WhenSerializingSupportedDevicesDataThenCorrectYamlIsGenerated) {
    SupportedDevicesMode mode = SupportedDevicesMode::concat;
    SupportedDevicesHelper supportedDevicesHelper(mode, argHelper->productConfigHelper.get());

    auto supportedDevicesData = supportedDevicesHelper.collectSupportedDevicesData(enabledDevices);

    std::string serializedData = supportedDevicesHelper.serialize("ocloc-test", supportedDevicesData);

    auto family1 = argHelper->productConfigHelper->getAcronymFromAFamily(mockDevices[0].family);
    auto family2 = argHelper->productConfigHelper->getAcronymFromAFamily(mockDevices[1].family);
    auto release1 = argHelper->productConfigHelper->getAcronymFromARelease(mockDevices[0].release);
    auto release2 = argHelper->productConfigHelper->getAcronymFromARelease(mockDevices[1].release);

    char expectedYaml[1024];
    snprintf(expectedYaml, sizeof(expectedYaml),
             R"(ocloc-test:
  device_ip_versions:
    - 0x1000001
    - 0x2000002
  ip_to_dev_rev_id:
    - ip: 0x1000001
      revision_id: 1
      device_id: 0x1111
    - ip: 0x1000001
      revision_id: 1
      device_id: 0x1100
    - ip: 0x2000002
      revision_id: 2
      device_id: 0x2222
    - ip: 0x2000002
      revision_id: 2
      device_id: 0x2200
  acronym:
    aaa: 0x1000001
    bbb: 0x1000001
    ccc: 0x2000002
    ddd: 0x2000002
  family_groups:
    %s: [0x1000001]
    %s: [0x2000002]
  release_groups:
    %s: [0x1000001]
    %s: [0x2000002]
)",
             family1.data(), family2.data(), release1.data(), release2.data());

    EXPECT_EQ(serializedData, expectedYaml);

    NEO::Yaml::YamlParser parser;
    std::string errReason, warning;
    EXPECT_TRUE(parser.parse(serializedData, errReason, warning));
    EXPECT_TRUE(errReason.empty());
}

TEST_F(SupportedDevicesHelperTest, WhenDeserializingSupportedDevicesDataThenAllFieldsAreCorrectlyPopulated) {
    SupportedDevicesMode mode = SupportedDevicesMode::concat;
    SupportedDevicesHelper supportedDevicesHelper(mode, argHelper->productConfigHelper.get());

    std::string yamlData = R"(ocloc-test:
  device_ip_versions:
    - 0x1000001
    - 0x2000002
  ip_to_dev_rev_id:
    - ip: 0x1000001
      revision_id: 1
      device_id: 0x1111
    - ip: 0x1000001
      revision_id: 1
      device_id: 0x1100
    - ip: 0x2000002
      revision_id: 2
      device_id: 0x2222
    - ip: 0x2000002
      revision_id: 2
      device_id: 0x2200
  acronym:
    aaa: 0x1000001
    bbb: 0x1000001
    ccc: 0x2000002
    ddd: 0x2000002
  family_groups:
    FAMILY1: [0x1000001, 0x1000002, 0x1000003]
    FAMILY2: [0x2000002, 0x2000003]
  release_groups:
    RELEASE1: [0x1000001, 0x1000002]
    RELEASE2: [0x2000002]
)";

    auto deserializedData = supportedDevicesHelper.deserialize(yamlData);

    ASSERT_EQ(deserializedData.size(), 1u);
    ASSERT_TRUE(deserializedData.find("ocloc-test") != deserializedData.end());

    auto &data = deserializedData["ocloc-test"];

    // Verify device_ip_versions
    ASSERT_EQ(data.deviceIpVersions.size(), 2u);
    EXPECT_EQ(data.deviceIpVersions[0], 0x1000001u);
    EXPECT_EQ(data.deviceIpVersions[1], 0x2000002u);

    // Verify ip_to_dev_rev_id
    ASSERT_EQ(data.deviceInfos.size(), 4u);
    EXPECT_EQ(data.deviceInfos[0].ipVersion, 0x1000001u);
    EXPECT_EQ(data.deviceInfos[0].revisionId, 1u);
    EXPECT_EQ(data.deviceInfos[0].deviceId, 0x1111u);
    EXPECT_EQ(data.deviceInfos[3].ipVersion, 0x2000002u);
    EXPECT_EQ(data.deviceInfos[3].revisionId, 2u);
    EXPECT_EQ(data.deviceInfos[3].deviceId, 0x2200u);

    // Verify acronyms
    ASSERT_EQ(data.acronyms.size(), 4u);
    EXPECT_EQ(data.acronyms[0].first, "aaa");
    EXPECT_EQ(data.acronyms[0].second, 0x1000001u);
    EXPECT_EQ(data.acronyms[3].first, "ddd");
    EXPECT_EQ(data.acronyms[3].second, 0x2000002u);

    // Verify familyGroups
    ASSERT_EQ(data.familyGroups.size(), 2u);
    EXPECT_EQ(data.familyGroups[0].first, "FAMILY1");
    EXPECT_EQ(data.familyGroups[0].second.size(), 3u);
    EXPECT_EQ(data.familyGroups[0].second[0], 0x1000001u);
    EXPECT_EQ(data.familyGroups[0].second[1], 0x1000002u);
    EXPECT_EQ(data.familyGroups[0].second[2], 0x1000003u);

    EXPECT_EQ(data.familyGroups[1].first, "FAMILY2");
    EXPECT_EQ(data.familyGroups[1].second.size(), 2u);
    EXPECT_EQ(data.familyGroups[1].second[0], 0x2000002u);
    EXPECT_EQ(data.familyGroups[1].second[1], 0x2000003u);

    // Verify releaseGroups
    ASSERT_EQ(data.releaseGroups.size(), 2u);
    EXPECT_EQ(data.releaseGroups[0].first, "RELEASE1");
    EXPECT_EQ(data.releaseGroups[0].second.size(), 2u);
    EXPECT_EQ(data.releaseGroups[0].second[0], 0x1000001u);
    EXPECT_EQ(data.releaseGroups[0].second[1], 0x1000002u);

    EXPECT_EQ(data.releaseGroups[1].first, "RELEASE2");
    EXPECT_EQ(data.releaseGroups[1].second.size(), 1u);
    EXPECT_EQ(data.releaseGroups[1].second[0], 0x2000002u);
}

TEST_F(SupportedDevicesHelperTest, WhenSerializingDeserializingAndSerializingAgainThenResultsAreIdentical) {
    SupportedDevicesMode mode = SupportedDevicesMode::concat;
    SupportedDevicesHelper supportedDevicesHelper(mode, argHelper->productConfigHelper.get());

    auto initialData = supportedDevicesHelper.collectSupportedDevicesData(enabledDevices);

    std::string firstSerialization = supportedDevicesHelper.serialize("ocloc-test", initialData);

    auto deserializedData = supportedDevicesHelper.deserialize(firstSerialization);
    ASSERT_EQ(deserializedData.size(), 1u);
    ASSERT_TRUE(deserializedData.find("ocloc-test") != deserializedData.end());

    std::string secondSerialization = supportedDevicesHelper.serialize("ocloc-test", deserializedData["ocloc-test"]);

    EXPECT_EQ(firstSerialization, secondSerialization);

    auto secondDeserializationData = supportedDevicesHelper.deserialize(secondSerialization);
    ASSERT_EQ(secondDeserializationData.size(), 1u);
    ASSERT_TRUE(secondDeserializationData.find("ocloc-test") != secondDeserializationData.end());

    const auto &finalData = secondDeserializationData["ocloc-test"];

    EXPECT_EQ(initialData.deviceIpVersions, finalData.deviceIpVersions);

    ASSERT_EQ(initialData.deviceInfos.size(), finalData.deviceInfos.size());
    for (size_t i = 0; i < initialData.deviceInfos.size(); ++i) {
        EXPECT_EQ(initialData.deviceInfos[i].deviceId, finalData.deviceInfos[i].deviceId);
        EXPECT_EQ(initialData.deviceInfos[i].revisionId, finalData.deviceInfos[i].revisionId);
        EXPECT_EQ(initialData.deviceInfos[i].ipVersion, finalData.deviceInfos[i].ipVersion);
    }

    ASSERT_EQ(initialData.acronyms.size(), finalData.acronyms.size());
    for (size_t i = 0; i < initialData.acronyms.size(); ++i) {
        EXPECT_EQ(initialData.acronyms[i].first, finalData.acronyms[i].first);
        EXPECT_EQ(initialData.acronyms[i].second, finalData.acronyms[i].second);
    }

    ASSERT_EQ(initialData.familyGroups.size(), finalData.familyGroups.size());
    for (size_t i = 0; i < initialData.familyGroups.size(); ++i) {
        EXPECT_EQ(initialData.familyGroups[i].first, finalData.familyGroups[i].first);
        EXPECT_EQ(initialData.familyGroups[i].second, finalData.familyGroups[i].second);
    }

    ASSERT_EQ(initialData.releaseGroups.size(), finalData.releaseGroups.size());
    for (size_t i = 0; i < initialData.releaseGroups.size(); ++i) {
        EXPECT_EQ(initialData.releaseGroups[i].first, finalData.releaseGroups[i].first);
        EXPECT_EQ(initialData.releaseGroups[i].second, finalData.releaseGroups[i].second);
    }
}

TEST_F(SupportedDevicesHelperTest, WhenMergingOclocDataThenAllDataIsCombinedCorrectly) {
    SupportedDevicesMode mode = SupportedDevicesMode::merge;
    MockSupportedDevicesHelper supportedDevicesHelper(mode, argHelper->productConfigHelper.get());

    std::map<std::string, SupportedDevicesHelper::SupportedDevicesData> versionDataMap;

    SupportedDevicesHelper::SupportedDevicesData data1;
    SupportedDevicesHelper::SupportedDevicesData data2;

    data1.deviceIpVersions = {0x1000001, 0x1000002};
    data2.deviceIpVersions = {0x1000001, 0x2000001};

    data1.deviceInfos = {{0x1111, 1, 0x1000001}, {0x2222, 2, 0x1000002}};
    data2.deviceInfos = {{0x1111, 1, 0x1000001}, {0x3333, 3, 0x2000001}};

    data1.acronyms = {{"aaa", 0x1000001}, {"bbb", 0x1000002}};
    data2.acronyms = {{"aaa", 0x1000001}, {"ccc", 0x2000001}};

    data1.familyGroups = {{"FAMILY1", {0x1000001}}, {"FAMILY2", {0x1000002}}};
    data2.familyGroups = {{"FAMILY1", {0x1000001, 0x2000001}}, {"FAMILY3", {0x2000001}}};

    data1.releaseGroups = {{"RELEASE1", {0x1000001}}, {"RELEASE2", {0x1000002}}};
    data2.releaseGroups = {{"RELEASE3", {0x1000001}}, {"RELEASE4", {0x2000001}}};

    versionDataMap["ocloc-1.0"] = data1;
    versionDataMap["ocloc-2.0"] = data2;

    auto mergedData = supportedDevicesHelper.mergeOclocData(versionDataMap);

    ASSERT_EQ(mergedData.deviceIpVersions.size(), 3u);
    EXPECT_TRUE(std::is_sorted(mergedData.deviceIpVersions.begin(), mergedData.deviceIpVersions.end()));
    EXPECT_EQ(mergedData.deviceIpVersions, std::vector<uint32_t>({0x1000001, 0x1000002, 0x2000001}));

    ASSERT_EQ(mergedData.deviceInfos.size(), 3u);
    EXPECT_TRUE(std::is_sorted(mergedData.deviceInfos.begin(), mergedData.deviceInfos.end(),
                               [](const auto &a, const auto &b) { return a.ipVersion < b.ipVersion; }));
    EXPECT_EQ(mergedData.deviceInfos[0].deviceId, 0x1111u);
    EXPECT_EQ(mergedData.deviceInfos[0].revisionId, 1u);
    EXPECT_EQ(mergedData.deviceInfos[0].ipVersion, 0x1000001u);

    EXPECT_EQ(mergedData.deviceInfos[1].deviceId, 0x2222u);
    EXPECT_EQ(mergedData.deviceInfos[1].revisionId, 2u);
    EXPECT_EQ(mergedData.deviceInfos[1].ipVersion, 0x1000002u);

    EXPECT_EQ(mergedData.deviceInfos[2].deviceId, 0x3333u);
    EXPECT_EQ(mergedData.deviceInfos[2].revisionId, 3u);
    EXPECT_EQ(mergedData.deviceInfos[2].ipVersion, 0x2000001u);

    ASSERT_EQ(mergedData.acronyms.size(), 3u);
    EXPECT_TRUE(std::is_sorted(mergedData.acronyms.begin(), mergedData.acronyms.end(),
                               [](const auto &a, const auto &b) { return a.second < b.second; }));
    EXPECT_EQ(mergedData.acronyms[0], (std::pair<std::string, uint32_t>{"aaa", 0x1000001}));
    EXPECT_EQ(mergedData.acronyms[1], (std::pair<std::string, uint32_t>{"bbb", 0x1000002}));
    EXPECT_EQ(mergedData.acronyms[2], (std::pair<std::string, uint32_t>{"ccc", 0x2000001}));

    ASSERT_EQ(mergedData.familyGroups.size(), 3u);
    EXPECT_TRUE(std::is_sorted(mergedData.familyGroups.begin(), mergedData.familyGroups.end(),
                               [](const auto &a, const auto &b) { return a.first < b.first; }));
    EXPECT_EQ(mergedData.familyGroups[0].first, "FAMILY1");
    EXPECT_EQ(mergedData.familyGroups[0].second, std::vector<uint32_t>({0x1000001, 0x2000001}));
    EXPECT_EQ(mergedData.familyGroups[1].first, "FAMILY2");
    EXPECT_EQ(mergedData.familyGroups[1].second, std::vector<uint32_t>({0x1000002}));
    EXPECT_EQ(mergedData.familyGroups[2].first, "FAMILY3");
    EXPECT_EQ(mergedData.familyGroups[2].second, std::vector<uint32_t>({0x2000001}));

    ASSERT_EQ(mergedData.releaseGroups.size(), 4u);
    EXPECT_TRUE(std::is_sorted(mergedData.releaseGroups.begin(), mergedData.releaseGroups.end(),
                               [](const auto &a, const auto &b) { return a.first < b.first; }));
    EXPECT_EQ(mergedData.releaseGroups[0].first, "RELEASE1");
    EXPECT_EQ(mergedData.releaseGroups[0].second, std::vector<uint32_t>({0x1000001}));
    EXPECT_EQ(mergedData.releaseGroups[1].first, "RELEASE2");
    EXPECT_EQ(mergedData.releaseGroups[1].second, std::vector<uint32_t>({0x1000002}));
    EXPECT_EQ(mergedData.releaseGroups[2].first, "RELEASE3");
    EXPECT_EQ(mergedData.releaseGroups[2].second, std::vector<uint32_t>({0x1000001}));
    EXPECT_EQ(mergedData.releaseGroups[3].first, "RELEASE4");
    EXPECT_EQ(mergedData.releaseGroups[3].second, std::vector<uint32_t>({0x2000001}));
}

TEST_F(SupportedDevicesHelperTest, WhenConcatAndSerializeWithFormerDataThenResultIsCorrect) {
    SupportedDevicesMode mode = SupportedDevicesMode::concat;
    MockSupportedDevicesHelper supportedDevicesHelper(mode, argHelper->productConfigHelper.get());

    auto currentData = supportedDevicesHelper.collectSupportedDevicesData(enabledDevices);

    std::string concatResult = supportedDevicesHelper.concatAndSerializeWithFormerData(currentData);

    auto family1 = argHelper->productConfigHelper->getAcronymFromAFamily(mockDevices[0].family);
    auto family2 = argHelper->productConfigHelper->getAcronymFromAFamily(mockDevices[1].family);
    auto release1 = argHelper->productConfigHelper->getAcronymFromARelease(mockDevices[0].release);
    auto release2 = argHelper->productConfigHelper->getAcronymFromARelease(mockDevices[1].release);

    char expectedYaml[2048];
    snprintf(expectedYaml, sizeof(expectedYaml),
             R"(ocloc-current:
  device_ip_versions:
    - 0x1000001
    - 0x2000002
  ip_to_dev_rev_id:
    - ip: 0x1000001
      revision_id: 1
      device_id: 0x1111
    - ip: 0x1000001
      revision_id: 1
      device_id: 0x1100
    - ip: 0x2000002
      revision_id: 2
      device_id: 0x2222
    - ip: 0x2000002
      revision_id: 2
      device_id: 0x2200
  acronym:
    aaa: 0x1000001
    bbb: 0x1000001
    ccc: 0x2000002
    ddd: 0x2000002
  family_groups:
    %s: [0x1000001]
    %s: [0x2000002]
  release_groups:
    %s: [0x1000001]
    %s: [0x2000002]

ocloc-former:
  device_ip_versions:
    - 0x3000001
    - 0x3000002
  ip_to_dev_rev_id:
    - ip: 0x3000001
      revision_id: 1
      device_id: 0x3333
    - ip: 0x3000002
      revision_id: 2
      device_id: 0x4444
  acronym:
    eee: 0x3000001
    fff: 0x3000002
  family_groups:
    FAMILY_FORMER: [0x3000001, 0x3000002]
  release_groups:
    RELEASE_FORMER: [0x3000001, 0x3000002]
)",
             family1.data(), family2.data(), release1.data(), release2.data());

    EXPECT_EQ(concatResult, expectedYaml);
}

TEST_F(SupportedDevicesHelperTest, GivenEmptyFormerDataWhenConcatAndSerializeWithFormerDataThenResultIsCorrect) {
    SupportedDevicesMode mode = SupportedDevicesMode::concat;
    MockSupportedDevicesHelper supportedDevicesHelper(mode, argHelper->productConfigHelper.get());
    supportedDevicesHelper.getDataFromFormerOclocVersionEmptyResult = true;

    auto currentData = supportedDevicesHelper.collectSupportedDevicesData(enabledDevices);

    std::string concatResult = supportedDevicesHelper.concatAndSerializeWithFormerData(currentData);

    auto family1 = argHelper->productConfigHelper->getAcronymFromAFamily(mockDevices[0].family);
    auto family2 = argHelper->productConfigHelper->getAcronymFromAFamily(mockDevices[1].family);
    auto release1 = argHelper->productConfigHelper->getAcronymFromARelease(mockDevices[0].release);
    auto release2 = argHelper->productConfigHelper->getAcronymFromARelease(mockDevices[1].release);

    char expectedYaml[2048];
    snprintf(expectedYaml, sizeof(expectedYaml),
             R"(ocloc-current:
  device_ip_versions:
    - 0x1000001
    - 0x2000002
  ip_to_dev_rev_id:
    - ip: 0x1000001
      revision_id: 1
      device_id: 0x1111
    - ip: 0x1000001
      revision_id: 1
      device_id: 0x1100
    - ip: 0x2000002
      revision_id: 2
      device_id: 0x2222
    - ip: 0x2000002
      revision_id: 2
      device_id: 0x2200
  acronym:
    aaa: 0x1000001
    bbb: 0x1000001
    ccc: 0x2000002
    ddd: 0x2000002
  family_groups:
    %s: [0x1000001]
    %s: [0x2000002]
  release_groups:
    %s: [0x1000001]
    %s: [0x2000002]
)",
             family1.data(), family2.data(), release1.data(), release2.data());

    EXPECT_EQ(concatResult, expectedYaml);
}

TEST_F(SupportedDevicesHelperTest, WhenMergeAndSerializeWithFormerDataThenBothVersionsAreCorrectlyMerged) {
    SupportedDevicesMode mode = SupportedDevicesMode::merge;
    MockSupportedDevicesHelper supportedDevicesHelper(mode, argHelper->productConfigHelper.get());
    supportedDevicesHelper.getDataFromFormerOclocVersionEmptyResult = false;

    auto currentData = supportedDevicesHelper.collectSupportedDevicesData(enabledDevices);
    std::string mergeResult = supportedDevicesHelper.mergeAndSerializeWithFormerData(currentData);

    auto deserializedResult = supportedDevicesHelper.deserialize(mergeResult);
    ASSERT_EQ(deserializedResult.size(), 1u);
    ASSERT_TRUE(deserializedResult.find("ocloc") != deserializedResult.end());
    const auto &mergedData = deserializedResult["ocloc"];

    std::vector<uint32_t> expectedIpVersions = {0x1000001, 0x2000002, 0x3000001, 0x3000002};
    EXPECT_EQ(mergedData.deviceIpVersions.size(), expectedIpVersions.size());
    for (const auto &ipVersion : expectedIpVersions) {
        EXPECT_NE(std::find(mergedData.deviceIpVersions.begin(), mergedData.deviceIpVersions.end(), ipVersion),
                  mergedData.deviceIpVersions.end());
    }

    std::vector<SupportedDevicesHelper::DeviceInfo> expectedDeviceInfos = {
        {0x1111, 1, 0x1000001},
        {0x1100, 1, 0x1000001},
        {0x2222, 2, 0x2000002},
        {0x2200, 2, 0x2000002},
        {0x3333, 1, 0x3000001},
        {0x4444, 2, 0x3000002}};
    EXPECT_EQ(mergedData.deviceInfos.size(), expectedDeviceInfos.size());
    for (const auto &info : expectedDeviceInfos) {
        auto it = std::find_if(mergedData.deviceInfos.begin(), mergedData.deviceInfos.end(),
                               [&](const SupportedDevicesHelper::DeviceInfo &di) {
                                   return di.deviceId == info.deviceId && di.revisionId == info.revisionId && di.ipVersion == info.ipVersion;
                               });
        EXPECT_NE(it, mergedData.deviceInfos.end());
    }

    std::vector<std::pair<std::string, uint32_t>> expectedAcronyms = {
        {"aaa", 0x1000001}, {"bbb", 0x1000001}, {"ccc", 0x2000002}, {"ddd", 0x2000002}, {"eee", 0x3000001}, {"fff", 0x3000002}};
    EXPECT_EQ(mergedData.acronyms.size(), expectedAcronyms.size());
    for (const auto &acronym : expectedAcronyms) {
        auto it = std::find(mergedData.acronyms.begin(), mergedData.acronyms.end(), acronym);
        EXPECT_NE(it, mergedData.acronyms.end());
    }

    auto family1 = argHelper->productConfigHelper->getAcronymFromAFamily(mockDevices[0].family);
    auto family2 = argHelper->productConfigHelper->getAcronymFromAFamily(mockDevices[1].family);
    std::vector<std::pair<std::string, std::vector<uint32_t>>> expectedFamilyGroups = {
        {family1.data(), {0x1000001}},
        {family2.data(), {0x2000002}},
        {"FAMILY_FORMER", {0x3000001, 0x3000002}}};
    EXPECT_EQ(mergedData.familyGroups.size(), expectedFamilyGroups.size());
    for (const auto &group : expectedFamilyGroups) {
        auto it = std::find_if(mergedData.familyGroups.begin(), mergedData.familyGroups.end(),
                               [&](const auto &fg) { return fg.first == group.first; });
        EXPECT_NE(it, mergedData.familyGroups.end());
        if (it != mergedData.familyGroups.end()) {
            EXPECT_EQ(it->second, group.second);
        }
    }

    auto release1 = argHelper->productConfigHelper->getAcronymFromARelease(mockDevices[0].release);
    auto release2 = argHelper->productConfigHelper->getAcronymFromARelease(mockDevices[1].release);
    std::vector<std::pair<std::string, std::vector<uint32_t>>> expectedReleaseGroups = {
        {release1.data(), {0x1000001}},
        {release2.data(), {0x2000002}},
        {"RELEASE_FORMER", {0x3000001, 0x3000002}}};
    EXPECT_EQ(mergedData.releaseGroups.size(), expectedReleaseGroups.size());
    for (const auto &group : expectedReleaseGroups) {
        auto it = std::find_if(mergedData.releaseGroups.begin(), mergedData.releaseGroups.end(),
                               [&](const auto &rg) { return rg.first == group.first; });
        EXPECT_NE(it, mergedData.releaseGroups.end());
        if (it != mergedData.releaseGroups.end()) {
            EXPECT_EQ(it->second, group.second);
        }
    }
}

TEST_F(SupportedDevicesHelperTest, GivenEmptyFormerDataWhenMergeAndSerializeWithFormerDataThenOnlyCurrentVersionIsPresent) {
    SupportedDevicesMode mode = SupportedDevicesMode::merge;
    MockSupportedDevicesHelper supportedDevicesHelper(mode, argHelper->productConfigHelper.get());
    supportedDevicesHelper.getDataFromFormerOclocVersionEmptyResult = true;

    auto currentData = supportedDevicesHelper.collectSupportedDevicesData(enabledDevices);
    std::string mergeResult = supportedDevicesHelper.mergeAndSerializeWithFormerData(currentData);

    auto family1 = argHelper->productConfigHelper->getAcronymFromAFamily(mockDevices[0].family);
    auto family2 = argHelper->productConfigHelper->getAcronymFromAFamily(mockDevices[1].family);
    auto release1 = argHelper->productConfigHelper->getAcronymFromARelease(mockDevices[0].release);
    auto release2 = argHelper->productConfigHelper->getAcronymFromARelease(mockDevices[1].release);

    char expectedYaml[2048];
    snprintf(expectedYaml, sizeof(expectedYaml),
             R"(ocloc-current:
  device_ip_versions:
    - 0x1000001
    - 0x2000002
  ip_to_dev_rev_id:
    - ip: 0x1000001
      revision_id: 1
      device_id: 0x1111
    - ip: 0x1000001
      revision_id: 1
      device_id: 0x1100
    - ip: 0x2000002
      revision_id: 2
      device_id: 0x2222
    - ip: 0x2000002
      revision_id: 2
      device_id: 0x2200
  acronym:
    aaa: 0x1000001
    bbb: 0x1000001
    ccc: 0x2000002
    ddd: 0x2000002
  family_groups:
    %s: [0x1000001]
    %s: [0x2000002]
  release_groups:
    %s: [0x1000001]
    %s: [0x2000002]
)",
             family1.data(), family2.data(), release1.data(), release2.data());

    EXPECT_EQ(mergeResult, expectedYaml);
}

TEST_F(SupportedDevicesHelperTest, GivenSupportedDevicesModeWhenCallingToStrThenCorrectStringIsReturned) {
    EXPECT_EQ("merge", toStr(SupportedDevicesMode::merge));
    EXPECT_EQ("concat", toStr(SupportedDevicesMode::concat));
    EXPECT_EQ("unknown", toStr(SupportedDevicesMode::unknown));
}

TEST_F(SupportedDevicesHelperTest, GivenInvalidYamlInputWhenDeserializeThenReturnEmptyData) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableDebugBreak.set(0);

    SupportedDevicesMode mode = SupportedDevicesMode::merge;
    SupportedDevicesHelper supportedDevicesHelper(mode, argHelper->productConfigHelper.get());

    ConstStringRef yaml =
        R"===(
  device_ip_versions:
    - 0x1000001
   - 0x2000002
  - 0x2000002
)===";

    auto ret = supportedDevicesHelper.deserialize(yaml.str());
    EXPECT_TRUE(ret.empty());
}

TEST_F(SupportedDevicesHelperTest, GivenYamlWithInvalidValuesThenReturnDataWithEmptySections) {
    SupportedDevicesHelper helper(SupportedDevicesMode::concat, nullptr);

    std::string incompleteYaml = R"(
ocloc-test:
  device_ip_versions:
    - not_a_number
  ip_to_dev_rev_id:
    - ip: 0x1000001
      revision_id: 1
    - ip: 0x1000001
      device_id: 0x1111
    - deviceId: 0x2222
      revisionId: 2
  acronym:
    aaa: not_a_number
  family_groups:
    FAMILY1:
      - not_a_number
  release_groups:
    RELEASE1:
      - not_a_number
)";

    auto result = helper.deserialize(incompleteYaml);

    ASSERT_EQ(result.size(), 1u);
    ASSERT_TRUE(result.find("ocloc-test") != result.end());
    const auto &data = result["ocloc-test"];

    EXPECT_TRUE(data.deviceIpVersions.empty());
    EXPECT_TRUE(data.deviceInfos.empty());
    EXPECT_TRUE(data.acronyms.empty());
    EXPECT_TRUE(data.familyGroups.empty());
    EXPECT_TRUE(data.releaseGroups.empty());
}

TEST_F(SupportedDevicesHelperTest, GivenYamlWithMissingSectionsWhenDeserializeThenReturnDataWithEmptySections) {
    SupportedDevicesHelper helper(SupportedDevicesMode::concat, nullptr);

    std::string missingYaml = R"(
ocloc-test:
)";

    auto result = helper.deserialize(missingYaml);

    ASSERT_EQ(result.size(), 1u);
    ASSERT_TRUE(result.find("ocloc-test") != result.end());
    const auto &data = result["ocloc-test"];

    EXPECT_TRUE(data.deviceIpVersions.empty());
    EXPECT_TRUE(data.deviceInfos.empty());
    EXPECT_TRUE(data.acronyms.empty());
    EXPECT_TRUE(data.familyGroups.empty());
    EXPECT_TRUE(data.releaseGroups.empty());
}

} // namespace NEO
