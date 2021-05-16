/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/ocloc_fatbinary_tests.h"

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/helpers/hw_helper.h"

#include <algorithm>
#include <unordered_set>

namespace NEO {

TEST(OclocFatBinaryRequestedFatBinary, WhenDeviceArgMissingThenReturnsFalse) {
    const char *args[] = {"ocloc", "-aaa", "*", "-device", "*"};

    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();

    EXPECT_FALSE(NEO::requestedFatBinary(0, nullptr, argHelper.get()));
    EXPECT_FALSE(NEO::requestedFatBinary(1, args, argHelper.get()));
    EXPECT_FALSE(NEO::requestedFatBinary(2, args, argHelper.get()));
    EXPECT_FALSE(NEO::requestedFatBinary(3, args, argHelper.get()));
    EXPECT_FALSE(NEO::requestedFatBinary(4, args, argHelper.get()));
}

TEST(OclocFatBinaryRequestedFatBinary, GivenDeviceArgProvidedWhenFatBinaryFormatWithRangeIsPassedThenTrueIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();

    const char *allPlatforms[] = {"ocloc", "-device", "*"};
    const char *manyPlatforms[] = {"ocloc", "-device", "a,b"};
    const char *manyGens[] = {"ocloc", "-device", "gen0,gen1"};
    const char *rangePlatformFrom[] = {"ocloc", "-device", "skl-"};
    const char *rangePlatformTo[] = {"ocloc", "-device", "-skl"};
    const char *rangePlatformBounds[] = {"ocloc", "-device", "skl-icllp"};
    const char *rangeGenFrom[] = {"ocloc", "-device", "gen0-"};
    const char *rangeGenTo[] = {"ocloc", "-device", "-gen5"};
    const char *rangeGenBounds[] = {"ocloc", "-device", "gen0-gen5"};

    EXPECT_TRUE(NEO::requestedFatBinary(3, allPlatforms, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, manyPlatforms, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, manyGens, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangePlatformFrom, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangePlatformTo, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangePlatformBounds, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangeGenFrom, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangeGenTo, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangeGenBounds, argHelper.get()));
}

TEST(OclocFatBinaryRequestedFatBinary, GivenDeviceArgProvidedWhenUnknownGenNameIsPassedThenRequestedFatBinaryReturnsFalse) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    const char *unknownGen[] = {"ocloc", "-device", "gen0"};
    const char *unknownGenCaseInsensitive[] = {"ocloc", "-device", "Gen0"};

    EXPECT_FALSE(NEO::requestedFatBinary(3, unknownGen, argHelper.get()));
    EXPECT_FALSE(NEO::requestedFatBinary(3, unknownGenCaseInsensitive, argHelper.get()));
}

TEST(OclocFatBinaryRequestedFatBinary, GivenDeviceArgProvidedWhenKnownGenNameIsPassedThenRequestedFatBinaryReturnsTrue) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    unsigned int i = 0;
    for (; i < IGFX_MAX_CORE; ++i) {
        if (NEO::familyName[i] != nullptr)
            break;
    }
    const char *genFromFamilyName[] = {"ocloc", "-device", NEO::familyName[i]};
    EXPECT_TRUE(NEO::requestedFatBinary(3, genFromFamilyName, argHelper.get()));
}

TEST(OclocFatBinaryRequestedFatBinary, WhenDeviceArgProvidedButDoesnNotContainFatbinaryArgFormatThenReturnsFalse) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    const char *skl[] = {"ocloc", "-device", "skl"};
    EXPECT_FALSE(NEO::requestedFatBinary(3, skl, argHelper.get()));
}

TEST(OclocFatBinaryGetAllSupportedTargetPlatforms, WhenRequestedThenReturnsAllPlatformsWithNonNullHardwarePrefixes) {
    auto platforms = NEO::getAllSupportedTargetPlatforms();
    std::unordered_set<uint32_t> platformsSet(platforms.begin(), platforms.end());
    for (unsigned int productId = 0; productId < IGFX_MAX_PRODUCT; ++productId) {
        if (nullptr != NEO::hardwarePrefix[productId]) {
            EXPECT_EQ(1U, platformsSet.count(static_cast<PRODUCT_FAMILY>(productId))) << productId;
        } else {
            EXPECT_EQ(0U, platformsSet.count(static_cast<PRODUCT_FAMILY>(productId))) << productId;
        }
    }
}

TEST(OclocFatBinaryToProductNames, GivenListOfProductIdsThenReturnsListOfHardwarePrefixes) {
    auto platforms = NEO::getAllSupportedTargetPlatforms();
    auto names = NEO::toProductNames(platforms);
    EXPECT_EQ(names.size(), platforms.size());
}

TEST(OclocFatBinaryAsProductId, GivenEnabledPlatformNameThenReturnsProperPlatformId) {
    auto platforms = NEO::getAllSupportedTargetPlatforms();
    auto names = NEO::toProductNames(platforms);
    for (size_t i = 0; i < platforms.size(); ++i) {
        auto idByName = NEO::asProductId(names[i], platforms);
        EXPECT_EQ(idByName, platforms[i]) << names[i].data() << " : " << platforms[i] << " != " << idByName;
    }
}

TEST(OclocFatBinaryAsProductId, GivenDisabledPlatformNameThenReturnsUnknownPlatformId) {
    auto platforms = NEO::getAllSupportedTargetPlatforms();
    auto names = NEO::toProductNames(platforms);
    platforms.clear();
    for (size_t i = 0; i < platforms.size(); ++i) {
        auto idByName = NEO::asProductId(names[i], platforms);
        EXPECT_EQ(IGFX_UNKNOWN, platforms[i]) << names[i].data() << " : IGFX_UNKNOWN != " << idByName;
    }
}

TEST(OclocFatBinaryAsGfxCoreIdList, GivenEnabledGfxCoreNameThenReturnsNonEmptyList) {

    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();

    for (unsigned int coreId = 0; coreId < IGFX_MAX_CORE; ++coreId) {
        if (nullptr != NEO::familyName[coreId]) {
            EXPECT_TRUE(argHelper->isGen(ConstStringRef(NEO::familyName[coreId]).str()));
            std::string caseInsensitive = NEO::familyName[coreId];
            std::transform(caseInsensitive.begin(), caseInsensitive.begin() + 1, caseInsensitive.begin(), ::tolower);
            EXPECT_TRUE(argHelper->isGen(caseInsensitive));
        }
    }
}

TEST(OclocFatBinaryAsGfxCoreIdList, GivenDisabledGfxCoreNameThenReturnsEmptyList) {

    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();

    EXPECT_FALSE(argHelper->isGen(ConstStringRef("genA").str()));
    EXPECT_FALSE(argHelper->isGen(ConstStringRef("gen0").str()));
    EXPECT_FALSE(argHelper->isGen(ConstStringRef("gen1").str()));
    EXPECT_FALSE(argHelper->isGen(ConstStringRef("gen2").str()));
}

TEST(OclocFatBinaryAsGfxCoreIdList, GivenEnabledGfxCoreNameThenReturnsNonNullIGFX) {

    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();

    for (unsigned int coreId = 0; coreId < IGFX_MAX_CORE; ++coreId) {
        if (nullptr != NEO::familyName[coreId]) {
            EXPECT_EQ(argHelper->returnIGFXforGen(ConstStringRef(NEO::familyName[coreId]).str()), coreId);
            std::string caseInsensitive = NEO::familyName[coreId];
            std::transform(caseInsensitive.begin(), caseInsensitive.begin() + 1, caseInsensitive.begin(), ::tolower);
            EXPECT_EQ(argHelper->returnIGFXforGen(caseInsensitive), coreId);
        }
    }
}

TEST(OclocFatBinaryAsGfxCoreIdList, GivenDisabledGfxCoreNameThenReturnsNullIGFX) {

    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();

    EXPECT_EQ(argHelper->returnIGFXforGen(ConstStringRef("genA").str()), 0u);
    EXPECT_EQ(argHelper->returnIGFXforGen(ConstStringRef("gen0").str()), 0u);
    EXPECT_EQ(argHelper->returnIGFXforGen(ConstStringRef("gen1").str()), 0u);
    EXPECT_EQ(argHelper->returnIGFXforGen(ConstStringRef("gen2").str()), 0u);
}

TEST(OclocFatBinaryAppendPlatformsForGfxCore, GivenCoreIdThenAppendsEnabledProductIdsThatMatch) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    auto platform0 = allEnabledPlatforms[0];
    auto gfxCore0 = NEO::hardwareInfoTable[platform0]->platform.eRenderCoreFamily;
    std::vector<PRODUCT_FAMILY> appendedPlatforms;
    NEO::appendPlatformsForGfxCore(gfxCore0, allEnabledPlatforms, appendedPlatforms);
    std::unordered_set<uint32_t> appendedPlatformsSet(appendedPlatforms.begin(), appendedPlatforms.end());
    EXPECT_EQ(1U, appendedPlatformsSet.count(platform0));
    for (auto platformId : allEnabledPlatforms) {
        if (gfxCore0 == NEO::hardwareInfoTable[platformId]->platform.eRenderCoreFamily) {
            EXPECT_EQ(1U, appendedPlatformsSet.count(platformId)) << platformId;
        } else {
            EXPECT_EQ(0U, appendedPlatformsSet.count(platformId)) << platformId;
        }
    }

    NEO::appendPlatformsForGfxCore(gfxCore0, allEnabledPlatforms, appendedPlatforms);
    EXPECT_EQ(2 * appendedPlatformsSet.size(), appendedPlatforms.size());
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenAsterixThenReturnAllEnabledPlatforms) {
    auto allEnabledPlatformsIds = NEO::getAllSupportedTargetPlatforms();
    auto expected = NEO::toProductNames(allEnabledPlatformsIds);
    auto got = NEO::getTargetPlatformsForFatbinary("*", oclocArgHelperWithoutInput.get());
    EXPECT_EQ(expected, got);
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenGenThenReturnAllEnabledPlatformsThatMatch) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    auto platform0 = allEnabledPlatforms[0];
    auto gfxCore0 = NEO::hardwareInfoTable[platform0]->platform.eRenderCoreFamily;
    std::string genName = NEO::familyName[gfxCore0];
    if (genName[0] == 'G') {
        genName[0] = 'g';
    }

    std::vector<PRODUCT_FAMILY> platformsForGen;
    NEO::appendPlatformsForGfxCore(gfxCore0, allEnabledPlatforms, platformsForGen);
    auto expected = NEO::toProductNames(platformsForGen);
    auto got = NEO::getTargetPlatformsForFatbinary(genName, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(expected, got);
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenMutiplePlatformThenReturnThosePlatforms) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    if (allEnabledPlatforms.size() < 2) {
        return;
    }
    auto platform0 = allEnabledPlatforms[0];
    std::string platform0Name = NEO::hardwarePrefix[platform0];
    auto platform1 = allEnabledPlatforms[1];
    std::string platform1Name = NEO::hardwarePrefix[platform1];

    std::vector<ConstStringRef> expected{platform0Name, platform1Name};
    auto got = NEO::getTargetPlatformsForFatbinary(platform0Name + "," + platform1Name, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(expected, got);
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenPlatformOpenRangeFromThenReturnAllEnabledPlatformsThatMatch) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    if (allEnabledPlatforms.size() < 3) {
        return;
    }
    auto platform0 = allEnabledPlatforms[allEnabledPlatforms.size() / 2];
    std::string platformName = NEO::hardwarePrefix[platform0];

    std::vector<PRODUCT_FAMILY> expectedPlatforms;
    auto platformFrom = std::find(allEnabledPlatforms.begin(), allEnabledPlatforms.end(), platform0);
    expectedPlatforms.insert(expectedPlatforms.end(), platformFrom, allEnabledPlatforms.end());
    auto expected = NEO::toProductNames(expectedPlatforms);
    auto got = NEO::getTargetPlatformsForFatbinary(platformName + "-", oclocArgHelperWithoutInput.get());
    EXPECT_EQ(expected, got);
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenPlatformOpenRangeToThenReturnAllEnabledPlatformsThatMatch) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    if (allEnabledPlatforms.size() < 3) {
        return;
    }
    auto platform0 = allEnabledPlatforms[allEnabledPlatforms.size() / 2];
    std::string platformName = NEO::hardwarePrefix[platform0];

    std::vector<PRODUCT_FAMILY> expectedPlatforms;
    auto platformTo = std::find(allEnabledPlatforms.begin(), allEnabledPlatforms.end(), platform0);
    expectedPlatforms.insert(expectedPlatforms.end(), allEnabledPlatforms.begin(), platformTo + 1);
    auto expected = NEO::toProductNames(expectedPlatforms);
    auto got = NEO::getTargetPlatformsForFatbinary("-" + platformName, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(expected, got);
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenPlatformClosedRangeThenReturnAllEnabledPlatformsThatMatch) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    if (allEnabledPlatforms.size() < 4) {
        return;
    }
    auto platformFrom = allEnabledPlatforms[1];
    auto platformTo = allEnabledPlatforms[allEnabledPlatforms.size() - 2];
    std::string platformNameFrom = NEO::hardwarePrefix[platformFrom];
    std::string platformNameTo = NEO::hardwarePrefix[platformTo];

    std::vector<PRODUCT_FAMILY> expectedPlatforms;
    expectedPlatforms.insert(expectedPlatforms.end(), allEnabledPlatforms.begin() + 1, allEnabledPlatforms.begin() + allEnabledPlatforms.size() - 1);
    auto expected = NEO::toProductNames(expectedPlatforms);
    auto got = NEO::getTargetPlatformsForFatbinary(platformNameFrom + "-" + platformNameTo, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(expected, got);

    got = NEO::getTargetPlatformsForFatbinary(platformNameTo + "-" + platformNameFrom, oclocArgHelperWithoutInput.get()); // swap min with max implicitly
    EXPECT_EQ(expected, got);
}

std::vector<GFXCORE_FAMILY> getEnabledCores() {
    std::vector<GFXCORE_FAMILY> ret;
    for (unsigned int coreId = 0; coreId < IGFX_MAX_CORE; ++coreId) {
        if (nullptr != NEO::familyName[coreId]) {
            ret.push_back(static_cast<GFXCORE_FAMILY>(coreId));
        }
    }
    return ret;
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenGenOpenRangeFromThenReturnAllEnabledPlatformsThatMatch) {
    auto allSupportedPlatforms = NEO::getAllSupportedTargetPlatforms();

    auto allEnabledCores = getEnabledCores();
    if (allEnabledCores.size() < 3) {
        return;
    }
    auto core0 = allEnabledCores[allEnabledCores.size() / 2];
    std::string genName = NEO::familyName[core0];
    if (genName[0] == 'G') {
        genName[0] = 'g';
    }

    std::vector<PRODUCT_FAMILY> expectedPlatforms;
    unsigned int coreIt = core0;
    while (coreIt < static_cast<unsigned int>(IGFX_MAX_CORE)) {
        NEO::appendPlatformsForGfxCore(static_cast<GFXCORE_FAMILY>(coreIt), allSupportedPlatforms, expectedPlatforms);
        ++coreIt;
    }

    auto expected = NEO::toProductNames(expectedPlatforms);
    auto got = NEO::getTargetPlatformsForFatbinary(genName + "-", oclocArgHelperWithoutInput.get());
    EXPECT_EQ(expected, got);
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenGenOpenRangeToThenReturnAllEnabledPlatformsThatMatch) {
    auto allSupportedPlatforms = NEO::getAllSupportedTargetPlatforms();

    auto allEnabledCores = getEnabledCores();
    if (allEnabledCores.size() < 3) {
        return;
    }
    auto core0 = allEnabledCores[allEnabledCores.size() / 2];
    std::string genName = NEO::familyName[core0];
    if (genName[0] == 'G') {
        genName[0] = 'g';
    }

    std::vector<PRODUCT_FAMILY> expectedPlatforms;
    unsigned int coreIt = IGFX_UNKNOWN_CORE;
    ++coreIt;
    while (coreIt <= static_cast<unsigned int>(core0)) {
        NEO::appendPlatformsForGfxCore(static_cast<GFXCORE_FAMILY>(coreIt), allSupportedPlatforms, expectedPlatforms);
        ++coreIt;
    }

    auto expected = NEO::toProductNames(expectedPlatforms);
    auto got = NEO::getTargetPlatformsForFatbinary("-" + genName, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(expected, got);
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenGenClosedRangeThenReturnAllEnabledPlatformsThatMatch) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    auto allEnabledCores = getEnabledCores();
    if (allEnabledCores.size() < 4) {
        return;
    }
    auto genFrom = allEnabledCores[1];
    auto genTo = allEnabledCores[allEnabledCores.size() - 2];
    std::string genNameFrom = NEO::familyName[genFrom];
    if (genNameFrom[0] == 'G') {
        genNameFrom[0] = 'g';
    }
    std::string genNameTo = NEO::familyName[genTo];
    if (genNameTo[0] == 'G') {
        genNameTo[0] = 'g';
    }

    std::vector<PRODUCT_FAMILY> expectedPlatforms;
    auto genIt = genFrom;
    while (genIt <= genTo) {
        NEO::appendPlatformsForGfxCore(static_cast<GFXCORE_FAMILY>(genIt), allEnabledPlatforms, expectedPlatforms);
        genIt = static_cast<GFXCORE_FAMILY>(static_cast<unsigned int>(genIt) + 1);
    }

    auto expected = NEO::toProductNames(expectedPlatforms);
    auto got = NEO::getTargetPlatformsForFatbinary(genNameFrom + "-" + genNameTo, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(expected, got);

    got = NEO::getTargetPlatformsForFatbinary(genNameTo + "-" + genNameFrom, oclocArgHelperWithoutInput.get()); // swap min with max implicitly
    EXPECT_EQ(expected, got);
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenUnkownGenThenReturnEmptyList) {
    auto got = NEO::getTargetPlatformsForFatbinary("gen0", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenMutiplePlatformWhenAnyOfPlatformsIsUnknownThenReturnEmptyList) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    auto platform0 = allEnabledPlatforms[0];
    std::string platform0Name = NEO::hardwarePrefix[platform0];

    auto got = NEO::getTargetPlatformsForFatbinary(platform0Name + ",unk", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenPlatformOpenRangeFromWhenPlatformsIsUnkownThenReturnEmptyList) {
    auto got = NEO::getTargetPlatformsForFatbinary("unk-", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenPlatformOpenRangeToWhenPlatformsIsUnkownThenReturnEmptyList) {
    auto got = NEO::getTargetPlatformsForFatbinary("-unk", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenPlatformClosedRangeWhenAnyOfPlatformsIsUnkownThenReturnEmptyList) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    auto platform0 = allEnabledPlatforms[0];
    std::string platform0Name = NEO::hardwarePrefix[platform0];

    auto got = NEO::getTargetPlatformsForFatbinary("unk-" + platform0Name, oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetPlatformsForFatbinary(platform0Name + "-unk", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenGenOpenRangeFromWhenGenIsUnknownThenReturnEmptyList) {
    auto got = NEO::getTargetPlatformsForFatbinary("gen2-", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenGenOpenRangeToWhenGenIsUnknownThenReturnEmptyList) {
    auto got = NEO::getTargetPlatformsForFatbinary("-gen2", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetPlatformsForFatbinary, GivenGenClosedRangeWhenAnyOfGensIsUnknownThenReturnEmptyList) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    auto platform0 = allEnabledPlatforms[0];
    auto gfxCore0 = NEO::hardwareInfoTable[platform0]->platform.eRenderCoreFamily;
    std::string genName = NEO::familyName[gfxCore0];
    if (genName[0] == 'G') {
        genName[0] = 'g';
    }

    auto got = NEO::getTargetPlatformsForFatbinary("gen2-" + genName, oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetPlatformsForFatbinary(genName + "-gen2", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}
} // namespace NEO
