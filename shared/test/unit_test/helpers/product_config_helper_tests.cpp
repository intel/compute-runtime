/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/product_config_helper_tests.h"

#include "shared/source/helpers/hw_info.h"
#include "shared/source/utilities/const_stringref.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

#include "neo_aot_platforms.h"

#include <algorithm>

TEST_F(ProductConfigHelperTests, givenFamilyEnumWhenHelperSearchForAMatchThenCorrespondingAcronymIsReturned) {
    for (const auto &[acronym, value] : AOT::familyAcronyms) {
        EXPECT_EQ(ProductConfigHelper::getAcronymFromAFamily(value), acronym);
    }
}

TEST_F(ProductConfigHelperTests, givenUnknownFamilyEnumWhenHelperSearchForAMatchThenEmptyAcronymIsReturned) {
    auto acronym = ProductConfigHelper::getAcronymFromAFamily(AOT::UNKNOWN_FAMILY);
    EXPECT_TRUE(acronym.empty());
}

TEST_F(ProductConfigHelperTests, givenReleaseEnumWhenHelperSearchForAMatchThenCorrespondingAcronymIsReturned) {
    auto productConfigHelper = std::make_unique<ProductConfigHelper>();
    for (const auto &[acronym, value] : AOT::releaseAcronyms) {
        if (acronym == "xe-lp") { // only case of multiple acronyms, for backward compatibility
            continue;
        }
        auto acronymFromARelease = ProductConfigHelper::getAcronymFromARelease(value);
        auto retrievedRelease = productConfigHelper->getReleaseFromDeviceName(acronymFromARelease.str());
        EXPECT_EQ(retrievedRelease, value);
    }
}

TEST_F(ProductConfigHelperTests, givenUnknownReleaseEnumWhenHelperSearchForAMatchThenEmptyAcronymIsReturned) {
    auto acronym = ProductConfigHelper::getAcronymFromARelease(AOT::UNKNOWN_RELEASE);
    EXPECT_TRUE(acronym.empty());
}

TEST_F(ProductConfigHelperTests, givenUnknownIsaEnumWhenParseMajorMinorRevisionValueThenCorrectStringIsReturned) {
    auto unknownIsa = ProductConfigHelper::parseMajorMinorRevisionValue(AOT::UNKNOWN_ISA);
    EXPECT_STREQ(unknownIsa.c_str(), "0.0.0");
}

TEST_F(ProductConfigHelperTests, givenDeviceStringWithCoreWhenAdjustDeviceNameThenCorrectStringIsReturned) {
    std::string deviceName = "gen0_core";
    ProductConfigHelper::adjustDeviceName(deviceName);
    EXPECT_STREQ(deviceName.c_str(), "gen0");
}

TEST_F(ProductConfigHelperTests, givenDeviceStringWithUnderscoreWhenAdjustDeviceNameThenCorrectStringIsReturned) {
    std::string deviceName = "ab_cd_ef";
    ProductConfigHelper::adjustDeviceName(deviceName);
    EXPECT_STREQ(deviceName.c_str(), "abcdef");
}

TEST_F(ProductConfigHelperTests, givenDeviceStringWithUnderscoreAndCoreWhenAdjustDeviceNameThenCorrectStringIsReturned) {
    std::string deviceName = "ab_cd_ef_core";
    ProductConfigHelper::adjustDeviceName(deviceName);
    EXPECT_STREQ(deviceName.c_str(), "abcdef");
}

TEST_F(ProductConfigHelperTests, givenDeviceStringWitDashesWhenAdjustDeviceNameThenTheSameStringIsReturned) {
    std::string deviceName = "ab-cd-ef";
    ProductConfigHelper::adjustDeviceName(deviceName);
    EXPECT_STREQ(deviceName.c_str(), "ab-cd-ef");
}

TEST_F(ProductConfigHelperTests, givenDeviceStringWithUnderscoreAndCapitalLetterWhenAdjustDeviceNameThenCorrectStringIsReturned) {
    std::string deviceName = "AB_cd_core";
    ProductConfigHelper::adjustDeviceName(deviceName);
    EXPECT_STREQ(deviceName.c_str(), "abcd");
}

TEST_F(ProductConfigHelperTests, givenProductAcronymWhenAdjustDeviceNameThenNothingIsChangedAndSameStringIsPreserved) {
    for (const auto &product : AOT::deviceAcronyms) {
        std::string acronymCopy = product.first;
        ProductConfigHelper::adjustDeviceName(acronymCopy);
        EXPECT_STREQ(acronymCopy.c_str(), product.first.c_str());
    }
}

TEST_F(ProductConfigHelperTests, givenReleaseAcronymWhenAdjustDeviceNameThenNothingIsChangedAndSameStringIsPreserved) {
    for (const auto &release : AOT::releaseAcronyms) {
        std::string acronymCopy = release.first;
        ProductConfigHelper::adjustDeviceName(acronymCopy);
        EXPECT_STREQ(acronymCopy.c_str(), release.first.c_str());
    }
}

TEST_F(ProductConfigHelperTests, givenFamilyAcronymWhenAdjustDeviceNameThenNothingIsChangedAndSameStringIsPreserved) {
    for (const auto &family : AOT::familyAcronyms) {
        std::string acronymCopy = family.first;
        ProductConfigHelper::adjustDeviceName(acronymCopy);
        EXPECT_STREQ(acronymCopy.c_str(), family.first.c_str());
    }
}

TEST_F(AotDeviceInfoTests, givenGen12lpFamilyAcronymWhenAdjustClosedRangeDeviceNamesThenProperReleaseAcronymsAreAssigned) {
    if (productConfigHelper->getReleasesAcronyms().size() < 2) {
        GTEST_SKIP();
    }
    std::map<AOT::FAMILY, AOT::RELEASE> familyToReleaseAcronyms = {{AOT::XE_FAMILY, AOT::XE_LP_RELEASE}};

    if (productConfigHelper->isSupportedRelease(AOT::XE_LPGPLUS_RELEASE)) {
        familyToReleaseAcronyms[AOT::XE_FAMILY] = AOT::XE_LPGPLUS_RELEASE;
    } else if (productConfigHelper->isSupportedRelease(AOT::XE_LPG_RELEASE)) {
        familyToReleaseAcronyms[AOT::XE_FAMILY] = AOT::XE_LPG_RELEASE;
    } else if (productConfigHelper->isSupportedRelease(AOT::XE_HPC_VG_RELEASE)) {
        familyToReleaseAcronyms[AOT::XE_FAMILY] = AOT::XE_HPC_VG_RELEASE;
    } else if (productConfigHelper->isSupportedRelease(AOT::XE_HPG_RELEASE)) {
        familyToReleaseAcronyms[AOT::XE_FAMILY] = AOT::XE_HPG_RELEASE;
    }

    EXPECT_EQ(productConfigHelper->getFamilyFromDeviceName("gen12lp"), AOT::UNKNOWN_FAMILY);
    for (const auto &[family, release] : familyToReleaseAcronyms) {
        std::string acronymFrom = "gen12lp";
        std::string acronymTo = ProductConfigHelper::getAcronymFromAFamily(family).str();
        productConfigHelper->adjustClosedRangeDeviceLegacyAcronyms(acronymFrom, acronymTo);
        EXPECT_STREQ(acronymTo.c_str(), ProductConfigHelper::getAcronymFromARelease(release).str().c_str());

        acronymFrom = ProductConfigHelper::getAcronymFromAFamily(family).str();
        acronymTo = "gen12lp";
        productConfigHelper->adjustClosedRangeDeviceLegacyAcronyms(acronymFrom, acronymTo);
        EXPECT_STREQ(acronymFrom.c_str(), ProductConfigHelper::getAcronymFromARelease(release).str().c_str());
    }
}

TEST_F(AotDeviceInfoTests, givenFamilyAcronymsWithoutGen12lpWhenAdjustClosedRangeDeviceNamesThenNothingIsChanged) {
    for (const auto &[acronymFrom, value] : AOT::familyAcronyms) {
        std::ignore = value;
        for (const auto &[acronymTo, value] : AOT::familyAcronyms) {
            std::ignore = value;
            std::string adjustedAcronymFrom = acronymFrom;
            std::string adjustedAcronymTo = acronymTo;
            productConfigHelper->adjustClosedRangeDeviceLegacyAcronyms(adjustedAcronymFrom, adjustedAcronymTo);
            EXPECT_STREQ(acronymTo.c_str(), adjustedAcronymTo.c_str());
            EXPECT_STREQ(acronymFrom.c_str(), adjustedAcronymFrom.c_str());
        }
    }
}

TEST_F(AotDeviceInfoTests, givenReleaseAcronymsWhenAdjustClosedRangeDeviceNamesThenNothingIsChanged) {
    for (const auto &[acronymFrom, value] : AOT::releaseAcronyms) {
        std::ignore = value;
        for (const auto &[acronymTo, value] : AOT::releaseAcronyms) {
            std::ignore = value;
            std::string adjustedAcronymFrom = acronymFrom;
            std::string adjustedAcronymTo = acronymTo;
            productConfigHelper->adjustClosedRangeDeviceLegacyAcronyms(adjustedAcronymFrom, adjustedAcronymTo);
            EXPECT_STREQ(acronymTo.c_str(), adjustedAcronymTo.c_str());
            EXPECT_STREQ(acronymFrom.c_str(), adjustedAcronymFrom.c_str());
        }
    }
}

TEST_F(ProductConfigHelperTests, givenAcronymWithoutDashesWhenSearchMatchInSampleVectorThenCorrectValueIsReturned) {
    std::vector<NEO::ConstStringRef> sampleAcronyms = {"ab-cd", "abc-p", "abc"};

    std::string acronym = "ab-cd";
    auto ret = std::find_if(sampleAcronyms.begin(), sampleAcronyms.end(), ProductConfigHelper::findAcronymWithoutDash(acronym));
    EXPECT_NE(ret, sampleAcronyms.end());

    acronym = "abcd";
    ret = std::find_if(sampleAcronyms.begin(), sampleAcronyms.end(), ProductConfigHelper::findAcronymWithoutDash(acronym));
    EXPECT_NE(ret, sampleAcronyms.end());

    acronym = "ab";
    ret = std::find_if(sampleAcronyms.begin(), sampleAcronyms.end(), ProductConfigHelper::findAcronymWithoutDash(acronym));
    EXPECT_EQ(ret, sampleAcronyms.end());

    acronym = "abdc";
    ret = std::find_if(sampleAcronyms.begin(), sampleAcronyms.end(), ProductConfigHelper::findAcronymWithoutDash(acronym));
    EXPECT_EQ(ret, sampleAcronyms.end());

    acronym = "abcp";
    ret = std::find_if(sampleAcronyms.begin(), sampleAcronyms.end(), ProductConfigHelper::findAcronymWithoutDash(acronym));
    EXPECT_NE(ret, sampleAcronyms.end());
}

TEST_F(ProductConfigHelperTests, givenProductConfigValueWhenParseVersionThenCorrectValueIsReturned) {
    for (const auto &configMap : AOT::deviceAcronyms) {
        auto version = ProductConfigHelper::parseMajorMinorRevisionValue(configMap.second);
        auto productConfig = ProductConfigHelper::getProductConfigFromVersionValue(version);
        EXPECT_EQ(productConfig, configMap.second);
    }
}

TEST_F(ProductConfigHelperTests, givenIncorrectVersionValueWhenGetProductConfigThenUnknownIsaIsReturned) {
    EXPECT_EQ(ProductConfigHelper::getProductConfigFromVersionValue("9.1."), AOT::UNKNOWN_ISA);
    EXPECT_EQ(ProductConfigHelper::getProductConfigFromVersionValue("9.1.."), AOT::UNKNOWN_ISA);
    EXPECT_EQ(ProductConfigHelper::getProductConfigFromVersionValue(".1.2"), AOT::UNKNOWN_ISA);
    EXPECT_EQ(ProductConfigHelper::getProductConfigFromVersionValue("9.0.a"), AOT::UNKNOWN_ISA);
    EXPECT_EQ(ProductConfigHelper::getProductConfigFromVersionValue("9.a"), AOT::UNKNOWN_ISA);
    EXPECT_EQ(ProductConfigHelper::getProductConfigFromVersionValue("256.350"), AOT::UNKNOWN_ISA);
}

TEST_F(ProductConfigHelperTests, GivenDifferentAotConfigsInDeviceAotInfosWhenComparingThemThenFalseIsReturned) {
    DeviceAotInfo lhs{};
    DeviceAotInfo rhs{};
    ASSERT_TRUE(lhs == rhs);

    lhs.aotConfig = {AOT::getConfixMaxPlatform()};
    rhs.aotConfig = {AOT::UNKNOWN_ISA};

    EXPECT_FALSE(lhs == rhs);
}

TEST_F(ProductConfigHelperTests, GivenDifferentFamiliesInDeviceAotInfosWhenComparingThemThenFalseIsReturned) {
    DeviceAotInfo lhs{};
    DeviceAotInfo rhs{};
    ASSERT_TRUE(lhs == rhs);

    lhs.family = AOT::FAMILY_MAX;
    rhs.family = AOT::UNKNOWN_FAMILY;

    EXPECT_FALSE(lhs == rhs);
}

TEST_F(ProductConfigHelperTests, GivenDifferentReleasesInDeviceAotInfosWhenComparingThemThenFalseIsReturned) {
    DeviceAotInfo lhs{};
    DeviceAotInfo rhs{};
    ASSERT_TRUE(lhs == rhs);

    lhs.release = AOT::RELEASE_MAX;
    rhs.release = AOT::UNKNOWN_RELEASE;

    EXPECT_FALSE(lhs == rhs);
}

TEST_F(ProductConfigHelperTests, GivenDifferentHwInfoInDeviceAotInfosWhenComparingThemThenFalseIsReturned) {
    DeviceAotInfo lhs{};
    DeviceAotInfo rhs{};

    lhs.hwInfo = NEO::defaultHwInfo.get();
    EXPECT_FALSE(lhs == rhs);

    rhs.hwInfo = NEO::defaultHwInfo.get();
    ASSERT_TRUE(lhs == rhs);
}

TEST_F(AotDeviceInfoTests, givenProductAcronymWhenHelperSearchForAMatchThenCorrespondingValueIsReturned) {
    uint32_t numSupportedAcronyms = 0;
    for (const auto &[acronym, value] : AOT::deviceAcronyms) {
        if (!productConfigHelper->isSupportedProductConfig(value)) {
            continue;
        }
        numSupportedAcronyms++;
        EXPECT_EQ(productConfigHelper->getProductConfigFromDeviceName(acronym), value);
    }
    for (const auto &[acronym, value] : AOT::getRtlIdAcronyms()) {
        if (!productConfigHelper->isSupportedProductConfig(value)) {
            continue;
        }
        numSupportedAcronyms++;
        EXPECT_EQ(productConfigHelper->getProductConfigFromDeviceName(acronym), value);
    }
    EXPECT_LT(0u, numSupportedAcronyms);
}

TEST_F(AotDeviceInfoTests, givenGenericAcronymWhenHelperSearchForAMatchThenCorrespondingValueIsReturned) {
    for (const auto &[acronym, value] : AOT::genericIdAcronyms) {
        EXPECT_EQ(productConfigHelper->getProductConfigFromDeviceName(acronym), value) << acronym;
    }
}

TEST_F(AotDeviceInfoTests, givenProductIpVersionStringWhenHelperSearchForProductConfigThenCorrectValueIsReturned) {
    for (const auto &deviceConfig : AOT::deviceAcronyms) {
        if (!productConfigHelper->isSupportedProductConfig(deviceConfig.second)) {
            continue;
        }
        std::stringstream ipVersion;
        ipVersion << deviceConfig.second;
        EXPECT_EQ(productConfigHelper->getProductConfigFromDeviceName(ipVersion.str()), deviceConfig.second);
    }
    for (const auto &deviceConfig : AOT::getRtlIdAcronyms()) {
        if (!productConfigHelper->isSupportedProductConfig(deviceConfig.second)) {
            continue;
        }
        std::stringstream ipVersion;
        ipVersion << deviceConfig.second;
        EXPECT_EQ(productConfigHelper->getProductConfigFromDeviceName(ipVersion.str()), deviceConfig.second);
    }
}

TEST_F(AotDeviceInfoTests, givenNotExistingProductIpVersionStringWhenHelperSearchForProductConfigThenCorrectValueIsReturned) {
    EXPECT_EQ(productConfigHelper->getProductConfigFromDeviceName("1234"), AOT::UNKNOWN_ISA);
}

TEST_F(AotDeviceInfoTests, givenReleaseAcronymWhenHelperSearchForAMatchThenCorrespondingValueIsReturned) {
    for (const auto &[acronym, value] : AOT::releaseAcronyms) {
        EXPECT_EQ(productConfigHelper->getReleaseFromDeviceName(acronym), value);
    }
}

TEST_F(AotDeviceInfoTests, givenFamilyAcronymWhenHelperSearchForAMatchThenCorrespondingValueIsReturned) {
    for (const auto &[acronym, value] : AOT::familyAcronyms) {
        EXPECT_EQ(productConfigHelper->getFamilyFromDeviceName(acronym), value);
    }
}

TEST_F(AotDeviceInfoTests, givenUnknownAcronymWhenHelperSearchForAMatchThenUnknownEnumValueIsReturned) {
    EXPECT_EQ(productConfigHelper->getProductConfigFromDeviceName("unk"), AOT::UNKNOWN_ISA);
    EXPECT_EQ(productConfigHelper->getReleaseFromDeviceName("unk"), AOT::UNKNOWN_RELEASE);
    EXPECT_EQ(productConfigHelper->getFamilyFromDeviceName("unk"), AOT::UNKNOWN_FAMILY);
}

TEST_F(AotDeviceInfoTests, givenProductOrAotConfigWhenParseMajorMinorRevisionValueThenCorrectStringIsReturned) {
    for (const auto &device : aotInfos) {
        auto productConfig = static_cast<AOT::PRODUCT_CONFIG>(device.aotConfig.value);
        auto configStr0 = ProductConfigHelper::parseMajorMinorRevisionValue(productConfig);
        auto configStr1 = ProductConfigHelper::parseMajorMinorRevisionValue(device.aotConfig);
        EXPECT_STREQ(configStr0.c_str(), configStr1.c_str());

        auto gotCofig = ProductConfigHelper::getProductConfigFromVersionValue(configStr0);

        EXPECT_EQ(gotCofig, productConfig);
    }
}

TEST_F(AotDeviceInfoTests, givenProductAcronymWhenRemoveDashesFromTheNameThenStillCorrectValueIsReturned) {
    uint32_t numSupportedAcronyms = 0;
    for (const auto &[acronym, value] : AOT::deviceAcronyms) {
        if (!productConfigHelper->isSupportedProductConfig(value)) {
            continue;
        }
        numSupportedAcronyms++;
        std::string acronymCopy = acronym;

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }

        EXPECT_EQ(productConfigHelper->getProductConfigFromDeviceName(acronymCopy), value);
    }
    for (const auto &[acronym, value] : AOT::getRtlIdAcronyms()) {
        if (!productConfigHelper->isSupportedProductConfig(value)) {
            continue;
        }
        numSupportedAcronyms++;
        std::string acronymCopy = acronym;

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }

        EXPECT_EQ(productConfigHelper->getProductConfigFromDeviceName(acronymCopy), value);
    }
    EXPECT_LT(0u, numSupportedAcronyms);
}

TEST_F(AotDeviceInfoTests, givenReleaseAcronymWhenRemoveDashesFromTheNameThenStillCorrectValueIsReturned) {
    uint32_t numSupportedAcronyms = 0;
    for (const auto &[acronym, value] : AOT::releaseAcronyms) {
        if (!productConfigHelper->isSupportedRelease(value)) {
            continue;
        }
        numSupportedAcronyms++;
        std::string acronymCopy = acronym;

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }

        EXPECT_EQ(productConfigHelper->getReleaseFromDeviceName(acronymCopy), value);
    }
    EXPECT_LT(0u, numSupportedAcronyms);
}

TEST_F(AotDeviceInfoTests, givenFamilyAcronymWhenRemoveDashesFromTheNameThenStillCorrectValueIsReturned) {
    uint32_t numSupportedAcronyms = 0;
    for (const auto &[acronym, value] : AOT::familyAcronyms) {
        if (!productConfigHelper->isSupportedFamily(value)) {
            continue;
        }
        numSupportedAcronyms++;
        std::string acronymCopy = acronym;

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }

        EXPECT_EQ(productConfigHelper->getFamilyFromDeviceName(acronymCopy), value);
    }
    EXPECT_LT(0u, numSupportedAcronyms);
}

TEST_F(AotDeviceInfoTests, givenProductConfigAcronymWhenCheckAllEnabledThenCorrectValuesAreReturned) {
    auto &enabledDeviceConfigs = productConfigHelper->getDeviceAotInfo();
    if (enabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    for (auto &device : enabledDeviceConfigs) {
        std::string acronym("");

        if (!device.deviceAcronyms.empty()) {
            acronym = device.deviceAcronyms.front().str();
        } else if (!device.rtlIdAcronyms.empty()) {
            acronym = device.rtlIdAcronyms.front().str();
        }

        if (!acronym.empty()) {
            auto enabledAcronyms = productConfigHelper->getAllProductAcronyms();

            auto acronymFound = std::any_of(enabledAcronyms.begin(), enabledAcronyms.end(), findAcronym(acronym));
            EXPECT_TRUE(acronymFound);

            device.deviceAcronyms.clear();
            device.rtlIdAcronyms.clear();
            device.aotConfig.value = AOT::UNKNOWN_ISA;

            enabledAcronyms = productConfigHelper->getAllProductAcronyms();
            acronymFound = std::any_of(enabledAcronyms.begin(), enabledAcronyms.end(), findAcronym(acronym));

            EXPECT_FALSE(acronymFound);

            auto config = productConfigHelper->getProductConfigFromDeviceName(acronym);
            EXPECT_FALSE(productConfigHelper->isSupportedProductConfig(config));
            EXPECT_EQ(config, AOT::UNKNOWN_ISA);
        }
    }
}

TEST_F(AotDeviceInfoTests, givenReleaseAcronymWhenCheckAllEnabledThenCorrectValuesAreReturned) {
    auto &enabledDeviceConfigs = productConfigHelper->getDeviceAotInfo();
    if (enabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    std::string acronym("");
    auto enabledRelease = enabledDeviceConfigs[0].release;

    for (const auto &[name, value] : AOT::releaseAcronyms) {
        if (value == enabledRelease) {
            acronym = name;
        }
    }

    auto enabledReleases = productConfigHelper->getReleasesAcronyms();
    auto releaseFound = std::any_of(enabledReleases.begin(), enabledReleases.end(), findAcronym(acronym));

    EXPECT_TRUE(releaseFound);

    for (auto &device : enabledDeviceConfigs) {
        if (enabledRelease == device.release) {
            device.release = AOT::UNKNOWN_RELEASE;
        }
    }

    enabledReleases = productConfigHelper->getReleasesAcronyms();
    releaseFound = std::any_of(enabledReleases.begin(), enabledReleases.end(), findAcronym(acronym));

    EXPECT_FALSE(releaseFound);
    auto release = productConfigHelper->getReleaseFromDeviceName(acronym);
    EXPECT_FALSE(productConfigHelper->isSupportedRelease(release));
    EXPECT_EQ(release, AOT::UNKNOWN_RELEASE);
}

TEST_F(AotDeviceInfoTests, givenFamilyAcronymWhenCheckAllEnabledThenCorrectValuesAreReturned) {
    auto &enabledDeviceConfigs = productConfigHelper->getDeviceAotInfo();
    if (enabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    std::string acronym("");
    auto enabledFamily = enabledDeviceConfigs[0].family;

    for (const auto &[name, value] : AOT::familyAcronyms) {
        if (value == enabledFamily) {
            acronym = name;
        }
    }

    auto enabledFamilies = productConfigHelper->getFamiliesAcronyms();
    auto familyFound = std::any_of(enabledFamilies.begin(), enabledFamilies.end(), findAcronym(acronym));

    EXPECT_TRUE(familyFound);

    for (auto &device : enabledDeviceConfigs) {
        if (enabledFamily == device.family) {
            device.family = AOT::UNKNOWN_FAMILY;
        }
    }

    enabledFamilies = productConfigHelper->getFamiliesAcronyms();
    familyFound = std::any_of(enabledFamilies.begin(), enabledFamilies.end(), findAcronym(acronym));

    EXPECT_FALSE(familyFound);
    auto family = productConfigHelper->getFamilyFromDeviceName(acronym);
    EXPECT_FALSE(productConfigHelper->isSupportedFamily(family));
    EXPECT_EQ(family, AOT::UNKNOWN_FAMILY);
}

TEST_F(AotDeviceInfoTests, givenEnabledFamilyAcronymsWhenCheckIfIsFamilyThenTrueIsReturned) {
    auto enabledFamiliesAcronyms = productConfigHelper->getFamiliesAcronyms();
    for (const auto &acronym : enabledFamiliesAcronyms) {
        auto family = productConfigHelper->getFamilyFromDeviceName(acronym.str());
        EXPECT_TRUE(productConfigHelper->isSupportedFamily(family));
        EXPECT_NE(family, AOT::UNKNOWN_FAMILY);
    }
}

TEST_F(AotDeviceInfoTests, givenEnabledReleaseAcronymsWhenCheckIfIsSupportedReleaseThenTrueIsReturned) {
    auto enabledReleasesAcronyms = productConfigHelper->getReleasesAcronyms();
    for (const auto &acronym : enabledReleasesAcronyms) {
        auto release = productConfigHelper->getReleaseFromDeviceName(acronym.str());
        EXPECT_TRUE(productConfigHelper->isSupportedRelease(release));
        EXPECT_NE(release, AOT::UNKNOWN_RELEASE);
    }
}

TEST_F(AotDeviceInfoTests, givenDisabledFamilyOrReleaseWhenCheckIfSupportedThenFalseIsReturned) {
    auto gen0Release = productConfigHelper->getReleaseFromDeviceName(NEO::ConstStringRef("gen0").str());
    auto genXRelease = productConfigHelper->getReleaseFromDeviceName(NEO::ConstStringRef("genX").str());
    auto gen0Family = productConfigHelper->getFamilyFromDeviceName(NEO::ConstStringRef("gen0").str());
    auto genXFamily = productConfigHelper->getFamilyFromDeviceName(NEO::ConstStringRef("genX").str());

    EXPECT_EQ(gen0Release, AOT::UNKNOWN_RELEASE);
    EXPECT_EQ(genXRelease, AOT::UNKNOWN_RELEASE);
    EXPECT_EQ(gen0Family, AOT::UNKNOWN_FAMILY);
    EXPECT_EQ(genXFamily, AOT::UNKNOWN_FAMILY);

    EXPECT_FALSE(productConfigHelper->isSupportedFamily(gen0Family));
    EXPECT_FALSE(productConfigHelper->isSupportedFamily(genXFamily));
    EXPECT_FALSE(productConfigHelper->isSupportedRelease(gen0Release));
    EXPECT_FALSE(productConfigHelper->isSupportedRelease(genXRelease));
}

TEST_F(AotDeviceInfoTests, givenEnabledFamilyAcronymsWithoutDashesWhenCheckIfIsFamilyThenTrueIsReturned) {
    auto enabledFamiliesAcronyms = productConfigHelper->getFamiliesAcronyms();
    for (const auto &acronym : enabledFamiliesAcronyms) {
        std::string acronymCopy = acronym.str();

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }

        auto family = productConfigHelper->getFamilyFromDeviceName(acronymCopy);
        EXPECT_TRUE(productConfigHelper->isSupportedFamily(family));
        EXPECT_NE(family, AOT::UNKNOWN_FAMILY);
    }
}

TEST_F(AotDeviceInfoTests, givenEnabledReleaseAcronymsWithoutDashesWhenCheckIfIsReleaseThenTrueIsReturned) {
    auto enabledReleasesAcronyms = productConfigHelper->getReleasesAcronyms();
    for (const auto &acronym : enabledReleasesAcronyms) {
        std::string acronymCopy = acronym.str();

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }

        auto release = productConfigHelper->getReleaseFromDeviceName(acronymCopy);
        EXPECT_TRUE(productConfigHelper->isSupportedRelease(release));
        EXPECT_NE(release, AOT::UNKNOWN_RELEASE);
    }
}

TEST_F(AotDeviceInfoTests, givenEnabledProductAcronymsWithoutDashesWhenCheckIfIsSupportedConfigThenTrueIsReturned) {
    auto enabledProductsAcronyms = productConfigHelper->getAllProductAcronyms();
    for (const auto &acronym : enabledProductsAcronyms) {
        std::string acronymCopy = acronym.str();

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }

        auto config = productConfigHelper->getProductConfigFromDeviceName(acronymCopy);
        EXPECT_TRUE(productConfigHelper->isSupportedProductConfig(config));
        EXPECT_NE(config, AOT::UNKNOWN_ISA);
    }
}

TEST_F(AotDeviceInfoTests, givenDeprecatedAcronymsWhenSearchingPresenceInNewNamesThenFalseIsReturned) {
    auto deprecatedAcronyms = productConfigHelper->getDeprecatedAcronyms();

    for (const auto &acronym : deprecatedAcronyms) {
        std::string acronymCopy = acronym.str();
        ProductConfigHelper::adjustDeviceName(acronymCopy);

        auto release = productConfigHelper->getReleaseFromDeviceName(acronymCopy);
        auto family = productConfigHelper->getFamilyFromDeviceName(acronymCopy);
        auto config = productConfigHelper->getProductConfigFromDeviceName(acronymCopy);

        EXPECT_EQ(release, AOT::UNKNOWN_RELEASE);
        EXPECT_EQ(family, AOT::UNKNOWN_FAMILY);
        EXPECT_EQ(config, AOT::UNKNOWN_ISA);

        EXPECT_FALSE(productConfigHelper->isSupportedFamily(family));
        EXPECT_FALSE(productConfigHelper->isSupportedRelease(release));
        EXPECT_FALSE(productConfigHelper->isSupportedProductConfig(config));
    }
}

TEST_F(AotDeviceInfoTests, givenNotFullConfigWhenGetProductConfigThenUnknownIsaIsReturned) {
    if (aotInfos.empty()) {
        GTEST_SKIP();
    }
    auto aotConfig = aotInfos[0].aotConfig;
    std::stringstream majorString;
    majorString << aotConfig.architecture;
    auto major = majorString.str();

    auto aotValue0 = ProductConfigHelper::getProductConfigFromVersionValue(major);
    EXPECT_EQ(aotValue0, AOT::UNKNOWN_ISA);

    auto majorMinor = ProductConfigHelper::parseMajorMinorValue(aotConfig);
    auto aotValue1 = ProductConfigHelper::getProductConfigFromVersionValue(majorMinor);
    EXPECT_EQ(aotValue1, AOT::UNKNOWN_ISA);
}

TEST_F(AotDeviceInfoTests, givenEnabledProductsAcronymsAndVersionsWhenCheckIfSupportedProductConfigThenTrueIsReturned) {
    for (const auto &product : aotInfos) {
        auto configStr = ProductConfigHelper::parseMajorMinorRevisionValue(product.aotConfig);
        EXPECT_FALSE(configStr.empty());

        auto config = productConfigHelper->getProductConfigFromDeviceName(configStr);
        EXPECT_NE(config, AOT::UNKNOWN_ISA);
        EXPECT_TRUE(productConfigHelper->isSupportedProductConfig(config));

        for (const auto &acronym : product.deviceAcronyms) {
            config = productConfigHelper->getProductConfigFromDeviceName(acronym.str());
            EXPECT_NE(config, AOT::UNKNOWN_ISA);
            EXPECT_TRUE(productConfigHelper->isSupportedProductConfig(config));
        }
    }
}

TEST_F(AotDeviceInfoTests, givenUnknownIsaVersionWhenGetProductConfigThenCorrectResultIsReturned) {
    auto configStr = ProductConfigHelper::parseMajorMinorRevisionValue(AOT::UNKNOWN_ISA);
    auto config = productConfigHelper->getProductConfigFromDeviceName(configStr);
    EXPECT_EQ(config, AOT::UNKNOWN_ISA);
}

TEST_F(AotDeviceInfoTests, givenRepresentativeProductsAcronymsWhenSearchInAllProductAcronymsThenStringIsFound) {
    auto representativeAcronyms = productConfigHelper->getRepresentativeProductAcronyms();
    auto allProductAcronyms = productConfigHelper->getAllProductAcronyms();

    for (const auto &acronym : representativeAcronyms) {
        EXPECT_NE(std::find(allProductAcronyms.begin(), allProductAcronyms.end(), acronym), allProductAcronyms.end());
    }
}

TEST_F(AotDeviceInfoTests, givenOnlyOneStringWhenGetRepresentativeProductAcronymsThenCorrectResultIsReturned) {
    auto &aotInfos = productConfigHelper->getDeviceAotInfo();
    if (aotInfos.empty()) {
        GTEST_SKIP();
    }

    for (auto &aotInfo : aotInfos) {
        aotInfo.deviceAcronyms.clear();
        aotInfo.rtlIdAcronyms.clear();
    }
    std::string tmp("tmp");
    NEO::ConstStringRef tmpStr(tmp);
    aotInfos[0].rtlIdAcronyms.push_back(tmpStr);

    auto representativeAcronyms = productConfigHelper->getRepresentativeProductAcronyms();
    EXPECT_EQ(representativeAcronyms.size(), 1u);
    EXPECT_TRUE(representativeAcronyms.front() == tmpStr);
}

TEST_F(AotDeviceInfoTests, givenClearedProductAcronymWhenSearchInRepresentativeAcronymsThenFewerAcronymsAreFound) {
    auto &enabledProducts = productConfigHelper->getDeviceAotInfo();

    for (auto &product : enabledProducts) {
        if (!product.deviceAcronyms.empty() || !product.rtlIdAcronyms.empty()) {
            auto representativeAcronyms = productConfigHelper->getRepresentativeProductAcronyms();
            product.deviceAcronyms = {};
            product.rtlIdAcronyms = {};
            auto cutRepresentativeAcronyms = productConfigHelper->getRepresentativeProductAcronyms();
            EXPECT_LT(cutRepresentativeAcronyms.size(), representativeAcronyms.size());
        }
    }
}

TEST_F(AotDeviceInfoTests, givenProductConfigWhenGetDeviceAotInfoThenCorrectValuesAreReturned) {
    DeviceAotInfo aotInfo{};

    for (auto &product : aotInfos) {
        auto productConfig = static_cast<AOT::PRODUCT_CONFIG>(product.aotConfig.value);
        EXPECT_TRUE(productConfigHelper->getDeviceAotInfoForProductConfig(productConfig, aotInfo));
        EXPECT_TRUE(aotInfo == product);
    }
}

TEST_F(AotDeviceInfoTests, givenUnknownIsaWhenGetDeviceAotInfoThenFalseIsReturned) {
    DeviceAotInfo aotInfo{}, emptyInfo{};

    EXPECT_FALSE(productConfigHelper->getDeviceAotInfoForProductConfig(AOT::UNKNOWN_ISA, aotInfo));
    EXPECT_TRUE(aotInfo == emptyInfo);
}

TEST_F(AotDeviceInfoTests, givenDeviceAcronymsOrProductConfigWhenGetProductFamilyThenCorrectResultIsReturned) {
    for (const auto &product : aotInfos) {
        auto config = ProductConfigHelper::parseMajorMinorRevisionValue(product.aotConfig);
        auto productFamily = productConfigHelper->getProductFamilyFromDeviceName(config);
        EXPECT_EQ(productFamily, product.hwInfo->platform.eProductFamily);

        for (const auto &acronym : product.deviceAcronyms) {
            productFamily = productConfigHelper->getProductFamilyFromDeviceName(acronym.str());
            EXPECT_EQ(productFamily, product.hwInfo->platform.eProductFamily);
        }
    }
}

TEST_F(AotDeviceInfoTests, givenTmpStringWhenSearchForDeviceAcronymThenCorrectResultIsReturned) {
    auto &deviceAot = productConfigHelper->getDeviceAotInfo();
    if (deviceAot.empty()) {
        GTEST_SKIP();
    }
    auto &product = deviceAot[0];
    std::string tmpStr("tmp");
    product.deviceAcronyms.insert(product.deviceAcronyms.begin(), NEO::ConstStringRef(tmpStr));

    auto name = productConfigHelper->getAcronymForProductConfig(product.aotConfig.value);
    EXPECT_EQ(name, tmpStr);
}

TEST_F(AotDeviceInfoTests, givenTmpStringWhenSearchForRtlIdAcronymThenCorrectResultIsReturned) {
    auto &deviceAot = productConfigHelper->getDeviceAotInfo();
    if (deviceAot.empty()) {
        GTEST_SKIP();
    }
    auto &product = deviceAot[0];
    product.deviceAcronyms.clear();
    std::string tmpStr("tmp");
    product.rtlIdAcronyms.insert(product.rtlIdAcronyms.begin(), NEO::ConstStringRef(tmpStr));

    auto name = productConfigHelper->getAcronymForProductConfig(product.aotConfig.value);
    EXPECT_EQ(name, tmpStr);
}

TEST_F(AotDeviceInfoTests, givenDeprecatedDeviceAcronymsWhenGetProductFamilyThenUnknownIsReturned) {
    auto deprecatedAcronyms = productConfigHelper->getDeprecatedAcronyms();
    for (const auto &acronym : deprecatedAcronyms) {
        EXPECT_EQ(productConfigHelper->getProductFamilyFromDeviceName(acronym.str()), IGFX_UNKNOWN);
    }
}

TEST_F(AotDeviceInfoTests, givenStringWhenFindAcronymThenCorrectResultIsReturned) {
    std::string a("a"), b("b"), c("c");
    std::vector<DeviceAotInfo> deviceAotInfo{{}};

    deviceAotInfo[0].deviceAcronyms.push_back(NEO::ConstStringRef(a));
    deviceAotInfo[0].rtlIdAcronyms.push_back(NEO::ConstStringRef(b));

    EXPECT_TRUE(std::any_of(deviceAotInfo.begin(), deviceAotInfo.end(), ProductConfigHelper::findAcronym(a)));
    EXPECT_TRUE(std::any_of(deviceAotInfo.begin(), deviceAotInfo.end(), ProductConfigHelper::findAcronym(b)));
    EXPECT_FALSE(std::any_of(deviceAotInfo.begin(), deviceAotInfo.end(), ProductConfigHelper::findAcronym(c)));
}

TEST_F(AotDeviceInfoTests, givenProductConfigHelperWhenGetDeviceAcronymsThenCorrectResultsAreReturned) {
    auto acronyms = productConfigHelper->getDeviceAcronyms();
    for (const auto &acronym : AOT::deviceAcronyms) {
        EXPECT_TRUE(std::any_of(acronyms.begin(), acronyms.end(), findAcronym(acronym.first)));
    }
}

TEST_F(AotDeviceInfoTests, givenDeviceAcroynmsWhenSearchingForDeviceAcronymsForReleaseThenObjectIsFound) {
    for (const auto &device : AOT::deviceAcronyms) {
        auto it = std::find_if(aotInfos.begin(), aotInfos.end(), ProductConfigHelper::findProductConfig(device.second));
        if (it == aotInfos.end()) {
            continue;
        }
        EXPECT_TRUE(std::any_of(aotInfos.begin(), aotInfos.end(), ProductConfigHelper::findDeviceAcronymForRelease(it->release)));
    }
}

TEST_F(AotDeviceInfoTests, givenNoAcronymsWhenGetAcronymForProductConfigThenMajorMinorRevisionIsReturned) {
    auto &deviceAot = productConfigHelper->getDeviceAotInfo();
    if (deviceAot.empty()) {
        GTEST_SKIP();
    }
    for (auto &device : deviceAot) {
        device.deviceAcronyms.clear();
        device.rtlIdAcronyms.clear();
        auto name = productConfigHelper->getAcronymForProductConfig(device.aotConfig.value);
        auto expected = productConfigHelper->parseMajorMinorRevisionValue(device.aotConfig);
        EXPECT_STREQ(name.c_str(), expected.c_str());
    }
}

TEST_F(AotDeviceInfoTests, givenUnknownIsaWhenSearchForAnAcronymThenEmptyIsReturned) {
    auto name = productConfigHelper->getAcronymForProductConfig(AOT::UNKNOWN_ISA);
    EXPECT_TRUE(name.empty());
}
