/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/test/common/test_macros/test.h"

#include <algorithm>

struct OclocArgHelperTests : public ::testing::Test {
    OclocArgHelperTests() {
        argHelper = std::make_unique<OclocArgHelper>();
    }
    std::unique_ptr<OclocArgHelper> argHelper;
};

template <typename EqComparableT>
auto findAcronym(const EqComparableT &lhs) {
    return [&lhs](const auto &rhs) { return lhs == rhs; };
}

TEST_F(OclocArgHelperTests, givenProductOrAotConfigWhenParseMajorMinorRevisionValueThenCorrectStringIsReturned) {
    auto &enabledDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();
    if (enabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    for (const auto &device : enabledDeviceConfigs) {
        auto productConfig = static_cast<AOT::PRODUCT_CONFIG>(device.aotConfig.ProductConfig);
        auto configStr0 = ProductConfigHelper::parseMajorMinorRevisionValue(productConfig);
        auto configStr1 = ProductConfigHelper::parseMajorMinorRevisionValue(device.aotConfig);
        EXPECT_STREQ(configStr0.c_str(), configStr1.c_str());

        auto gotCofig = argHelper->getProductConfigForVersionValue(configStr0);

        EXPECT_EQ(gotCofig, productConfig);
    }
}

TEST_F(OclocArgHelperTests, givenProductConfigAcronymWhenCheckAllEnabledThenCorrectValuesAreReturned) {
    auto &enabledDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();
    if (enabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    std::string acronym("");
    for (auto &device : enabledDeviceConfigs) {
        if (!device.acronyms.empty()) {
            acronym = device.acronyms.front().str();
            auto enabledAcronyms = argHelper->getEnabledProductAcronyms();

            auto acronymFound = std::any_of(enabledAcronyms.begin(), enabledAcronyms.end(), findAcronym(acronym));
            EXPECT_TRUE(acronymFound);

            device.acronyms.clear();
            device.aotConfig.ProductConfig = AOT::UNKNOWN_ISA;

            enabledAcronyms = argHelper->getEnabledProductAcronyms();
            acronymFound = std::any_of(enabledAcronyms.begin(), enabledAcronyms.end(), findAcronym(acronym));

            EXPECT_FALSE(acronymFound);
            EXPECT_FALSE(argHelper->isProductConfig(acronym));
        }
    }
}

TEST_F(OclocArgHelperTests, givenReleaseAcronymWhenCheckAllEnabledThenCorrectValuesAreReturned) {
    auto &enabledDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();
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

    auto enabledReleases = argHelper->getEnabledReleasesAcronyms();
    auto releaseFound = std::any_of(enabledReleases.begin(), enabledReleases.end(), findAcronym(acronym));

    EXPECT_TRUE(releaseFound);

    for (auto &device : enabledDeviceConfigs) {
        if (enabledRelease == device.release) {
            device.release = AOT::UNKNOWN_RELEASE;
        }
    }

    enabledReleases = argHelper->getEnabledReleasesAcronyms();
    releaseFound = std::any_of(enabledReleases.begin(), enabledReleases.end(), findAcronym(acronym));

    EXPECT_FALSE(releaseFound);
    EXPECT_FALSE(argHelper->isRelease(acronym));
}

TEST_F(OclocArgHelperTests, givenFamilyAcronymWhenCheckAllEnabledThenCorrectValuesAreReturned) {
    auto &enabledDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();
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

    auto enabledFamilies = argHelper->getEnabledFamiliesAcronyms();
    auto familyFound = std::any_of(enabledFamilies.begin(), enabledFamilies.end(), findAcronym(acronym));

    EXPECT_TRUE(familyFound);

    for (auto &device : enabledDeviceConfigs) {
        if (enabledFamily == device.family) {
            device.family = AOT::UNKNOWN_FAMILY;
        }
    }

    enabledFamilies = argHelper->getEnabledFamiliesAcronyms();
    familyFound = std::any_of(enabledFamilies.begin(), enabledFamilies.end(), findAcronym(acronym));

    EXPECT_FALSE(familyFound);
    EXPECT_FALSE(argHelper->isFamily(acronym));
}

TEST_F(OclocArgHelperTests, givenHwInfoForProductConfigWhenUnknownIsaIsPassedThenFalseIsReturned) {
    NEO::HardwareInfo hwInfo;
    EXPECT_FALSE(argHelper->getHwInfoForProductConfig(AOT::UNKNOWN_ISA, hwInfo, 0u));
}

TEST_F(OclocArgHelperTests, givenEnabledFamilyAcronymsWhenCheckIfIsFamilyThenTrueIsReturned) {
    auto enabledFamiliesAcronyms = argHelper->getEnabledFamiliesAcronyms();
    for (const auto &acronym : enabledFamiliesAcronyms) {
        EXPECT_TRUE(argHelper->isFamily(acronym.str()));
    }
}

TEST_F(OclocArgHelperTests, givenEnabledReleaseAcronymsWhenCheckIfIsReleaseThenTrueIsReturned) {
    auto enabledReleasesAcronyms = argHelper->getEnabledReleasesAcronyms();
    for (const auto &acronym : enabledReleasesAcronyms) {
        EXPECT_TRUE(argHelper->isRelease(acronym.str()));
    }
}

TEST_F(OclocArgHelperTests, givenEnabledProductsAcronymsAndVersionsWhenCheckIfProductConfigThenTrueIsReturned) {
    auto enabledProducts = argHelper->getAllSupportedDeviceConfigs();
    for (const auto &product : enabledProducts) {
        auto configStr = ProductConfigHelper::parseMajorMinorRevisionValue(product.aotConfig);
        EXPECT_FALSE(configStr.empty());
        EXPECT_TRUE(argHelper->isProductConfig(configStr));

        for (const auto &acronym : product.acronyms) {
            EXPECT_TRUE(argHelper->isProductConfig(acronym.str()));
        }
    }
}

TEST_F(OclocArgHelperTests, givenUnknownIsaVersionWhenCheckIfProductConfigThenFalseIsReturned) {
    auto configStr = ProductConfigHelper::parseMajorMinorRevisionValue(AOT::UNKNOWN_ISA);
    EXPECT_FALSE(argHelper->isProductConfig(configStr));
}

TEST_F(OclocArgHelperTests, givenDisabledFamilyOrReleaseNameThenReturnsEmptyList) {
    EXPECT_FALSE(argHelper->isFamily(NEO::ConstStringRef("gen0").str()));
    EXPECT_FALSE(argHelper->isFamily(NEO::ConstStringRef("genX").str()));
    EXPECT_FALSE(argHelper->isRelease(NEO::ConstStringRef("gen0").str()));
    EXPECT_FALSE(argHelper->isRelease(NEO::ConstStringRef("genX").str()));
}

TEST_F(OclocArgHelperTests, givenEnabledFamilyAcronymsWithoutDashesWhenCheckIfIsFamilyThenTrueIsReturned) {
    auto enabledFamiliesAcronyms = argHelper->getEnabledFamiliesAcronyms();
    for (const auto &acronym : enabledFamiliesAcronyms) {
        std::string acronymCopy = acronym.str();

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }
        EXPECT_TRUE(argHelper->isFamily(acronymCopy));
    }
}

TEST_F(OclocArgHelperTests, givenEnabledReleaseAcronymsWithoutDashesWhenCheckIfIsReleaseThenTrueIsReturned) {
    auto enabledReleasesAcronyms = argHelper->getEnabledReleasesAcronyms();
    for (const auto &acronym : enabledReleasesAcronyms) {
        std::string acronymCopy = acronym.str();

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }
        EXPECT_TRUE(argHelper->isRelease(acronymCopy));
    }
}

TEST_F(OclocArgHelperTests, givenEnabledProductAcronymsWithoutDashesWhenCheckIfIsReleaseThenTrueIsReturned) {
    auto enabledProductsAcronyms = argHelper->getEnabledProductAcronyms();
    for (const auto &acronym : enabledProductsAcronyms) {
        std::string acronymCopy = acronym.str();

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }
        EXPECT_TRUE(argHelper->isProductConfig(acronymCopy));
    }
}