/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/utilities/const_stringref.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

#include "platforms.h"

#include <algorithm>

using ProductConfigHelperTests = ::testing::Test;

TEST_F(ProductConfigHelperTests, givenProductAcronymWhenHelperSearchForAMatchThenCorrespondingValueIsReturned) {
    for (const auto &[acronym, value] : AOT::productConfigAcronyms) {
        EXPECT_EQ(ProductConfigHelper::getProductConfigForAcronym(acronym), value);
    }
}

TEST_F(ProductConfigHelperTests, givenReleaseAcronymWhenHelperSearchForAMatchThenCorrespondingValueIsReturned) {
    for (const auto &[acronym, value] : AOT::releaseAcronyms) {
        EXPECT_EQ(ProductConfigHelper::getReleaseForAcronym(acronym), value);
    }
}

TEST_F(ProductConfigHelperTests, givenFamilyAcronymWhenHelperSearchForAMatchThenCorrespondingValueIsReturned) {
    for (const auto &[acronym, value] : AOT::familyAcronyms) {
        EXPECT_EQ(ProductConfigHelper::getFamilyForAcronym(acronym), value);
    }
}

TEST_F(ProductConfigHelperTests, givenUnknownAcronymWhenHelperSearchForAMatchThenUnknownEnumValueIsReturned) {
    EXPECT_EQ(ProductConfigHelper::getProductConfigForAcronym("unk"), AOT::UNKNOWN_ISA);
    EXPECT_EQ(ProductConfigHelper::getReleaseForAcronym("unk"), AOT::UNKNOWN_RELEASE);
    EXPECT_EQ(ProductConfigHelper::getFamilyForAcronym("unk"), AOT::UNKNOWN_FAMILY);
}

TEST_F(ProductConfigHelperTests, givenFamilyEnumWhenHelperSearchForAMatchThenCorrespondingAcronymIsReturned) {
    for (const auto &[acronym, value] : AOT::familyAcronyms) {
        EXPECT_EQ(ProductConfigHelper::getAcronymForAFamily(value), acronym);
    }
}

TEST_F(ProductConfigHelperTests, givenUnknownFamilyEnumWhenHelperSearchForAMatchThenEmptyAcronymIsReturned) {
    auto acronym = ProductConfigHelper::getAcronymForAFamily(AOT::UNKNOWN_FAMILY);
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
    for (const auto &product : AOT::productConfigAcronyms) {
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

TEST_F(ProductConfigHelperTests, givenProductAcronymWhenRemoveDashesFromTheNameThenStillCorrectValueIsReturned) {
    for (const auto &[acronym, value] : AOT::productConfigAcronyms) {
        std::string acronymCopy = acronym;

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }

        EXPECT_EQ(ProductConfigHelper::getProductConfigForAcronym(acronymCopy), value);
    }
}

TEST_F(ProductConfigHelperTests, givenReleaseAcronymWhenRemoveDashesFromTheNameThenStillCorrectValueIsReturned) {
    for (const auto &[acronym, value] : AOT::releaseAcronyms) {
        std::string acronymCopy = acronym;

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }

        EXPECT_EQ(ProductConfigHelper::getReleaseForAcronym(acronymCopy), value);
    }
}

TEST_F(ProductConfigHelperTests, givenFamilyAcronymWhenRemoveDashesFromTheNameThenStillCorrectValueIsReturned) {
    for (const auto &[acronym, value] : AOT::familyAcronyms) {
        std::string acronymCopy = acronym;

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }

        EXPECT_EQ(ProductConfigHelper::getFamilyForAcronym(acronymCopy), value);
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
    for (const auto &configMap : AOT::productConfigAcronyms) {
        auto version = ProductConfigHelper::parseMajorMinorRevisionValue(configMap.second);
        auto productConfig = ProductConfigHelper::getProductConfigForVersionValue(version);
        EXPECT_EQ(productConfig, configMap.second);
    }
}

TEST_F(ProductConfigHelperTests, givenIncorrectVersionValueWhenGetProductConfigThenUnknownIsaIsReturned) {
    EXPECT_EQ(ProductConfigHelper::getProductConfigForVersionValue("9.1."), AOT::UNKNOWN_ISA);
    EXPECT_EQ(ProductConfigHelper::getProductConfigForVersionValue("9.1.."), AOT::UNKNOWN_ISA);
    EXPECT_EQ(ProductConfigHelper::getProductConfigForVersionValue(".1.2"), AOT::UNKNOWN_ISA);
    EXPECT_EQ(ProductConfigHelper::getProductConfigForVersionValue("9.0.a"), AOT::UNKNOWN_ISA);
    EXPECT_EQ(ProductConfigHelper::getProductConfigForVersionValue("9.a"), AOT::UNKNOWN_ISA);
    EXPECT_EQ(ProductConfigHelper::getProductConfigForVersionValue("256.350"), AOT::UNKNOWN_ISA);
}

TEST_F(ProductConfigHelperTests, GivenDifferentAotConfigsInDeviceAotInfosWhenComparingThemThenFalseIsReturned) {
    DeviceAotInfo lhs{};
    DeviceAotInfo rhs{};
    ASSERT_TRUE(lhs == rhs);

    lhs.aotConfig = {AOT::CONFIG_MAX_PLATFORM};
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

struct AotDeviceInfoTests : public ::testing::Test {
    AotDeviceInfoTests() {
        productConfigHelper = std::make_unique<ProductConfigHelper>();
    }
    std::unique_ptr<ProductConfigHelper> productConfigHelper;
};

template <typename EqComparableT>
auto findAcronym(const EqComparableT &lhs) {
    return [&lhs](const auto &rhs) { return lhs == rhs; };
}

TEST_F(AotDeviceInfoTests, givenProductOrAotConfigWhenParseMajorMinorRevisionValueThenCorrectStringIsReturned) {
    auto &enabledDeviceConfigs = productConfigHelper->getDeviceAotInfo();
    if (enabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    for (const auto &device : enabledDeviceConfigs) {
        auto productConfig = static_cast<AOT::PRODUCT_CONFIG>(device.aotConfig.ProductConfig);
        auto configStr0 = ProductConfigHelper::parseMajorMinorRevisionValue(productConfig);
        auto configStr1 = ProductConfigHelper::parseMajorMinorRevisionValue(device.aotConfig);
        EXPECT_STREQ(configStr0.c_str(), configStr1.c_str());

        auto gotCofig = ProductConfigHelper::getProductConfigForVersionValue(configStr0);

        EXPECT_EQ(gotCofig, productConfig);
    }
}

TEST_F(AotDeviceInfoTests, givenProductConfigAcronymWhenCheckAllEnabledThenCorrectValuesAreReturned) {
    auto &enabledDeviceConfigs = productConfigHelper->getDeviceAotInfo();
    if (enabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    std::string acronym("");
    for (auto &device : enabledDeviceConfigs) {
        if (!device.acronyms.empty()) {
            acronym = device.acronyms.front().str();
            auto enabledAcronyms = productConfigHelper->getAllProductAcronyms();

            auto acronymFound = std::any_of(enabledAcronyms.begin(), enabledAcronyms.end(), findAcronym(acronym));
            EXPECT_TRUE(acronymFound);

            device.acronyms.clear();
            device.aotConfig.ProductConfig = AOT::UNKNOWN_ISA;

            enabledAcronyms = productConfigHelper->getAllProductAcronyms();
            acronymFound = std::any_of(enabledAcronyms.begin(), enabledAcronyms.end(), findAcronym(acronym));

            EXPECT_FALSE(acronymFound);
            EXPECT_FALSE(productConfigHelper->isProductConfig(acronym));
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
    EXPECT_FALSE(productConfigHelper->isRelease(acronym));
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
    EXPECT_FALSE(productConfigHelper->isFamily(acronym));
}

TEST_F(AotDeviceInfoTests, givenEnabledFamilyAcronymsWhenCheckIfIsFamilyThenTrueIsReturned) {
    auto enabledFamiliesAcronyms = productConfigHelper->getFamiliesAcronyms();
    for (const auto &acronym : enabledFamiliesAcronyms) {
        EXPECT_TRUE(productConfigHelper->isFamily(acronym.str()));
    }
}

TEST_F(AotDeviceInfoTests, givenEnabledReleaseAcronymsWhenCheckIfIsReleaseThenTrueIsReturned) {
    auto enabledReleasesAcronyms = productConfigHelper->getReleasesAcronyms();
    for (const auto &acronym : enabledReleasesAcronyms) {
        EXPECT_TRUE(productConfigHelper->isRelease(acronym.str()));
    }
}

TEST_F(AotDeviceInfoTests, givenDisabledFamilyOrReleaseNameThenReturnsEmptyList) {
    EXPECT_FALSE(productConfigHelper->isFamily(NEO::ConstStringRef("gen0").str()));
    EXPECT_FALSE(productConfigHelper->isFamily(NEO::ConstStringRef("genX").str()));
    EXPECT_FALSE(productConfigHelper->isRelease(NEO::ConstStringRef("gen0").str()));
    EXPECT_FALSE(productConfigHelper->isRelease(NEO::ConstStringRef("genX").str()));
}

TEST_F(AotDeviceInfoTests, givenEnabledFamilyAcronymsWithoutDashesWhenCheckIfIsFamilyThenTrueIsReturned) {
    auto enabledFamiliesAcronyms = productConfigHelper->getFamiliesAcronyms();
    for (const auto &acronym : enabledFamiliesAcronyms) {
        std::string acronymCopy = acronym.str();

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }
        EXPECT_TRUE(productConfigHelper->isFamily(acronymCopy));
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
        EXPECT_TRUE(productConfigHelper->isRelease(acronymCopy));
    }
}

TEST_F(AotDeviceInfoTests, givenEnabledProductAcronymsWithoutDashesWhenCheckIfIsReleaseThenTrueIsReturned) {
    auto enabledProductsAcronyms = productConfigHelper->getAllProductAcronyms();
    for (const auto &acronym : enabledProductsAcronyms) {
        std::string acronymCopy = acronym.str();

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }
        EXPECT_TRUE(productConfigHelper->isProductConfig(acronymCopy));
    }
}

TEST_F(AotDeviceInfoTests, givenDeprecatedAcronymsWhenSearchingPresenceInNewNamesThenFalseIsReturned) {
    auto deprecatedAcronyms = productConfigHelper->getDeprecatedAcronyms();

    for (const auto &acronym : deprecatedAcronyms) {
        std::string acronymCopy = acronym.str();
        ProductConfigHelper::adjustDeviceName(acronymCopy);

        EXPECT_FALSE(productConfigHelper->isFamily(acronymCopy));
        EXPECT_FALSE(productConfigHelper->isRelease(acronymCopy));
        EXPECT_FALSE(productConfigHelper->isProductConfig(acronymCopy));
    }
}

TEST_F(AotDeviceInfoTests, givenNotFullConfigWhenGetProductConfigThenUnknownIsaIsReturned) {
    auto allEnabledDeviceConfigs = productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }
    auto aotConfig = allEnabledDeviceConfigs[0].aotConfig;
    std::stringstream majorString;
    majorString << aotConfig.ProductConfigID.Major;
    auto major = majorString.str();

    auto aotValue0 = ProductConfigHelper::getProductConfigForVersionValue(major);
    EXPECT_EQ(aotValue0, AOT::UNKNOWN_ISA);

    auto majorMinor = ProductConfigHelper::parseMajorMinorValue(aotConfig);
    auto aotValue1 = ProductConfigHelper::getProductConfigForVersionValue(majorMinor);
    EXPECT_EQ(aotValue1, AOT::UNKNOWN_ISA);
}

TEST_F(AotDeviceInfoTests, givenEnabledProductsAcronymsAndVersionsWhenCheckIfProductConfigThenTrueIsReturned) {
    auto enabledProducts = productConfigHelper->getDeviceAotInfo();
    for (const auto &product : enabledProducts) {
        auto configStr = ProductConfigHelper::parseMajorMinorRevisionValue(product.aotConfig);
        EXPECT_FALSE(configStr.empty());
        EXPECT_TRUE(productConfigHelper->isProductConfig(configStr));

        for (const auto &acronym : product.acronyms) {
            EXPECT_TRUE(productConfigHelper->isProductConfig(acronym.str()));
        }
    }
}

TEST_F(AotDeviceInfoTests, givenUnknownIsaVersionWhenCheckIfProductConfigThenFalseIsReturned) {
    auto configStr = ProductConfigHelper::parseMajorMinorRevisionValue(AOT::UNKNOWN_ISA);
    EXPECT_FALSE(productConfigHelper->isProductConfig(configStr));
}

TEST_F(AotDeviceInfoTests, givenRepresentativeProductsAcronymsWhenSearchInAllProductAcronymsThenStringIsFound) {
    auto representativeAcronyms = productConfigHelper->getRepresentativeProductAcronyms();
    auto allProductAcronyms = productConfigHelper->getAllProductAcronyms();

    for (const auto &acronym : representativeAcronyms) {
        EXPECT_NE(std::find(allProductAcronyms.begin(), allProductAcronyms.end(), acronym), allProductAcronyms.end());
    }
}

TEST_F(AotDeviceInfoTests, givenClearedProductAcronymWhenSearchInRepresentativeAcronymsThenFewerAcronymsAreFound) {
    auto &enabledProducts = productConfigHelper->getDeviceAotInfo();

    for (auto &product : enabledProducts) {
        if (!product.acronyms.empty()) {
            auto representativeAcronyms = productConfigHelper->getRepresentativeProductAcronyms();
            product.acronyms = {};
            auto cutRepresentativeAcronyms = productConfigHelper->getRepresentativeProductAcronyms();
            EXPECT_LT(cutRepresentativeAcronyms.size(), representativeAcronyms.size());
        }
    }
}

TEST_F(AotDeviceInfoTests, givenProductConfigWhenGetDeviceAotInfoThenCorrectValuesAreReturned) {
    auto &enabledProducts = productConfigHelper->getDeviceAotInfo();
    DeviceAotInfo aotInfo{};

    for (auto &product : enabledProducts) {
        auto productConfig = static_cast<AOT::PRODUCT_CONFIG>(product.aotConfig.ProductConfig);
        EXPECT_TRUE(productConfigHelper->getDeviceAotInfoForProductConfig(productConfig, aotInfo));
        EXPECT_TRUE(aotInfo == product);
    }
}

TEST_F(AotDeviceInfoTests, givenUnknownIsaWhenGetDeviceAotInfoThenFalseIsReturned) {
    DeviceAotInfo aotInfo{}, emptyInfo{};

    EXPECT_FALSE(productConfigHelper->getDeviceAotInfoForProductConfig(AOT::UNKNOWN_ISA, aotInfo));
    EXPECT_TRUE(aotInfo == emptyInfo);
}
