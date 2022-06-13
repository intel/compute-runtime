/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/test_macros/test.h"

using ProductConfigHelperTests = ::testing::Test;

TEST_F(ProductConfigHelperTests, givenProductAcronymWhenHelperSearchForAMatchThenCorrespondingValueIsReturned) {
    for (const auto &product : AOT::productConfigAcronyms) {
        EXPECT_EQ(ProductConfigHelper::returnProductConfigForAcronym(product.first), product.second);
    }
}

TEST_F(ProductConfigHelperTests, givenReleaseAcronymWhenHelperSearchForAMatchThenCorrespondingValueIsReturned) {
    for (const auto &release : AOT::releaseAcronyms) {
        EXPECT_EQ(ProductConfigHelper::returnReleaseForAcronym(release.first), release.second);
    }
}

TEST_F(ProductConfigHelperTests, givenFamilyAcronymWhenHelperSearchForAMatchThenCorrespondingValueIsReturned) {
    for (const auto &family : AOT::familyAcronyms) {
        EXPECT_EQ(ProductConfigHelper::returnFamilyForAcronym(family.first), family.second);
    }
}

TEST_F(ProductConfigHelperTests, givenUnknownAcronymWhenHelperSearchForAMatchThenUnknownEnumValueIsReturned) {
    EXPECT_EQ(ProductConfigHelper::returnProductConfigForAcronym("unk"), AOT::UNKNOWN_ISA);
    EXPECT_EQ(ProductConfigHelper::returnReleaseForAcronym("unk"), AOT::UNKNOWN_RELEASE);
    EXPECT_EQ(ProductConfigHelper::returnFamilyForAcronym("unk"), AOT::UNKNOWN_FAMILY);
}

TEST_F(ProductConfigHelperTests, givenFamilyEnumWhenHelperSearchForAMatchThenCorrespondingAcronymIsReturned) {
    for (const auto &family : AOT::familyAcronyms) {
        EXPECT_EQ(ProductConfigHelper::getAcronymForAFamily(family.second), family.first);
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