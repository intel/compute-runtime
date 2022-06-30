/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "platforms.h"

using ProductConfigHelperTests = ::testing::Test;

TEST_F(ProductConfigHelperTests, givenProductAcronymWhenHelperSearchForAMatchThenCorrespondingValueIsReturned) {
    for (const auto &[acronym, value] : AOT::productConfigAcronyms) {
        EXPECT_EQ(ProductConfigHelper::returnProductConfigForAcronym(acronym), value);
    }
}

TEST_F(ProductConfigHelperTests, givenReleaseAcronymWhenHelperSearchForAMatchThenCorrespondingValueIsReturned) {
    for (const auto &[acronym, value] : AOT::releaseAcronyms) {
        EXPECT_EQ(ProductConfigHelper::returnReleaseForAcronym(acronym), value);
    }
}

TEST_F(ProductConfigHelperTests, givenFamilyAcronymWhenHelperSearchForAMatchThenCorrespondingValueIsReturned) {
    for (const auto &[acronym, value] : AOT::familyAcronyms) {
        EXPECT_EQ(ProductConfigHelper::returnFamilyForAcronym(acronym), value);
    }
}

TEST_F(ProductConfigHelperTests, givenUnknownAcronymWhenHelperSearchForAMatchThenUnknownEnumValueIsReturned) {
    EXPECT_EQ(ProductConfigHelper::returnProductConfigForAcronym("unk"), AOT::UNKNOWN_ISA);
    EXPECT_EQ(ProductConfigHelper::returnReleaseForAcronym("unk"), AOT::UNKNOWN_RELEASE);
    EXPECT_EQ(ProductConfigHelper::returnFamilyForAcronym("unk"), AOT::UNKNOWN_FAMILY);
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

        EXPECT_EQ(ProductConfigHelper::returnProductConfigForAcronym(acronymCopy), value);
    }
}

TEST_F(ProductConfigHelperTests, givenReleaseAcronymWhenRemoveDashesFromTheNameThenStillCorrectValueIsReturned) {
    for (const auto &[acronym, value] : AOT::releaseAcronyms) {
        std::string acronymCopy = acronym;

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }

        EXPECT_EQ(ProductConfigHelper::returnReleaseForAcronym(acronymCopy), value);
    }
}

TEST_F(ProductConfigHelperTests, givenFamilyAcronymWhenRemoveDashesFromTheNameThenStillCorrectValueIsReturned) {
    for (const auto &[acronym, value] : AOT::familyAcronyms) {
        std::string acronymCopy = acronym;

        auto findDash = acronymCopy.find("-");
        if (findDash != std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }

        EXPECT_EQ(ProductConfigHelper::returnFamilyForAcronym(acronymCopy), value);
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