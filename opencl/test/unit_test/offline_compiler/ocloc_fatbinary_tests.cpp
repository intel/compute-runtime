/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/ocloc_fatbinary_tests.h"

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/source/device_binary_format/ar/ar.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/hw_helper.h"

#include <algorithm>
#include <unordered_set>

namespace NEO {

auto searchInArchiveByFilename(const Ar::Ar &archive, const ConstStringRef &name) {
    const auto isSearchedFile = [&name](const auto &file) {
        return file.fileName == name;
    };

    const auto &arFiles = archive.files;
    return std::find_if(arFiles.begin(), arFiles.end(), isSearchedFile);
}

std::string prepareTwoDevices(MockOclocArgHelper *argHelper) {
    auto allEnabledDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();
    if (allEnabledDeviceConfigs.size() < 2) {
        return {};
    }

    const auto cfg1 = argHelper->parseProductConfigFromValue(allEnabledDeviceConfigs[0].config);
    const auto cfg2 = argHelper->parseProductConfigFromValue(allEnabledDeviceConfigs[1].config);

    return cfg1 + "," + cfg2;
}

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
    const char *rangeConfigBounds[] = {"ocloc", "-device", "9-11"};
    const char *manyConfigs[] = {"ocloc", "-device", "9.0,11"};
    const char *rangeConfigFrom[] = {"ocloc", "-device", "10.1-"};
    const char *rangeConfigTo[] = {"ocloc", "-device", "-11.2"};
    const char *rangeConfigsBoundsSecond[] = {"ocloc", "-device", "11.2-12.2"};

    EXPECT_TRUE(NEO::requestedFatBinary(3, allPlatforms, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, manyPlatforms, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, manyGens, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangePlatformFrom, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangePlatformTo, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangePlatformBounds, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangeGenFrom, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangeGenTo, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangeGenBounds, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangeConfigBounds, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, manyConfigs, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangeConfigFrom, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangeConfigTo, argHelper.get()));
    EXPECT_TRUE(NEO::requestedFatBinary(3, rangeConfigsBoundsSecond, argHelper.get()));
}

TEST(OclocFatBinaryRequestedFatBinary, GivenDeviceArgToFatBinaryWhenConfigMatchesMoreThanOneProductThenTrueIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();

    std::string configNum0 = argHelper->parseProductConfigFromValue(allEnabledDeviceConfigs[allEnabledDeviceConfigs.size() / 2].config);
    auto major_pos = configNum0.find(".");
    auto cutMinorAndRevision = configNum0.substr(0, major_pos);
    auto matchedConfigs = getAllMatchedConfigs(cutMinorAndRevision, argHelper.get());

    if (matchedConfigs.size() < 2) {
        GTEST_SKIP();
    }

    const char *fewConfigs[] = {"ocloc", "-device", cutMinorAndRevision.c_str()};
    EXPECT_TRUE(NEO::requestedFatBinary(3, fewConfigs, argHelper.get()));
}

TEST(OclocFatBinaryRequestedFatBinary, GivenDeviceArgAsSingleProductConfigThenFatBinaryIsNotRequested) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();

    for (auto &deviceConfig : allEnabledDeviceConfigs) {
        std::string configStr = argHelper->parseProductConfigFromValue(deviceConfig.config);
        const char *singleConfig[] = {"ocloc", "-device", configStr.c_str()};
        EXPECT_FALSE(NEO::requestedFatBinary(3, singleConfig, argHelper.get()));
    }
}

TEST(OclocFatBinaryRequestedFatBinary, WhenPlatformIsProvidedButDoesNotContainMoreThanOneProductThenReturnFalse) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    const char *skl[] = {"ocloc", "-device", "skl"};
    EXPECT_FALSE(NEO::requestedFatBinary(3, skl, argHelper.get()));
}

TEST(OclocFatBinaryToProductConfigStrings, GivenListOfProductIdsThenReturnsListOfStrings) {
    auto platforms = NEO::getAllSupportedTargetPlatforms();
    auto names = NEO::toProductNames(platforms);
    EXPECT_EQ(names.size(), platforms.size());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenDifferentDeviceArgWhenCheckIfPlatformsAbbreviationIsPassedThenReturnCorrectValue) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    auto allEnabledDeviceConfigs = oclocArgHelperWithoutInput->getAllSupportedDeviceConfigs();

    if (allEnabledPlatforms.size() < 3 || allEnabledDeviceConfigs.size() < 3) {
        GTEST_SKIP();
    }

    auto platform0 = allEnabledPlatforms[0];
    ConstStringRef platformName0(hardwarePrefix[platform0], strlen(hardwarePrefix[platform0]));
    auto platform1 = allEnabledPlatforms[1];
    ConstStringRef platformName1(hardwarePrefix[platform1], strlen(hardwarePrefix[platform1]));

    auto deviceMapConfig0 = allEnabledDeviceConfigs[0];
    auto configNumConvention0 = oclocArgHelperWithoutInput->parseProductConfigFromValue(deviceMapConfig0.config);
    auto deviceMapConfig1 = allEnabledDeviceConfigs[1];
    auto configNumConvention1 = oclocArgHelperWithoutInput->parseProductConfigFromValue(deviceMapConfig1.config);

    auto twoPlatforms = platformName0.str() + "," + platformName1.str();
    auto configsRange = configNumConvention0 + "-" + configNumConvention1;
    auto gen = std::to_string(deviceMapConfig0.hwInfo->platform.eRenderCoreFamily);

    EXPECT_TRUE(isDeviceWithPlatformAbbreviation(platformName0, oclocArgHelperWithoutInput.get()));
    EXPECT_TRUE(isDeviceWithPlatformAbbreviation(ConstStringRef(twoPlatforms), oclocArgHelperWithoutInput.get()));
    EXPECT_FALSE(isDeviceWithPlatformAbbreviation(ConstStringRef(configsRange), oclocArgHelperWithoutInput.get()));
    EXPECT_FALSE(isDeviceWithPlatformAbbreviation(ConstStringRef(gen), oclocArgHelperWithoutInput.get()));
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenAsterixThenReturnAllEnabledConfigs) {
    auto expected = oclocArgHelperWithoutInput->getAllSupportedDeviceConfigs();
    auto got = NEO::getTargetConfigsForFatbinary("*", oclocArgHelperWithoutInput.get());

    EXPECT_EQ(got.size(), expected.size());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenProductConfigWhenConfigIsUndefinedThenReturnEmptyList) {
    auto got = NEO::getTargetConfigsForFatbinary("0.0.0", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetConfigsForFatbinary("0.0", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetConfigsForFatbinary("0", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenProductConfigOpenRangeToWhenConfigIsUndefinedThenReturnEmptyList) {
    auto got = NEO::getTargetConfigsForFatbinary("-0.0.0", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetConfigsForFatbinary("-0.0", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetConfigsForFatbinary("-0", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenProductConfigOpenRangeFromWhenConfigIsUndefinedThenReturnEmptyList) {
    auto got = NEO::getTargetConfigsForFatbinary("0.0.0-", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetConfigsForFatbinary("0.0-", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetConfigsForFatbinary("0-", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenProductConfigClosedRangeWhenAnyOfConfigIsUndefinedOrIncorrectThenReturnEmptyList) {
    auto allEnabledDeviceConfigs = oclocArgHelperWithoutInput->getAllSupportedDeviceConfigs();
    if (allEnabledDeviceConfigs.size() < 2) {
        GTEST_SKIP();
    }

    auto deviceMapConfig0 = allEnabledDeviceConfigs[0];
    auto config0Str = oclocArgHelperWithoutInput->parseProductConfigFromValue(deviceMapConfig0.config);

    auto got = NEO::getTargetConfigsForFatbinary("1.2-" + config0Str, oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetConfigsForFatbinary(config0Str + "-1.2", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetConfigsForFatbinary("1.a.c-" + config0Str, oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetConfigsForFatbinary(config0Str + "-1.a.c", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
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
            std::transform(caseInsensitive.begin(), caseInsensitive.end(), caseInsensitive.begin(), ::tolower);
            EXPECT_TRUE(argHelper->isGen(caseInsensitive));

            auto findCore = caseInsensitive.find("_core");
            if (findCore != std::string::npos) {
                caseInsensitive = caseInsensitive.substr(0, findCore);
                EXPECT_TRUE(argHelper->isGen(caseInsensitive));
            }

            auto findUnderline = caseInsensitive.find("_");
            if (findUnderline != std::string::npos) {
                caseInsensitive.erase(std::remove(caseInsensitive.begin(), caseInsensitive.end(), '_'), caseInsensitive.end());
                EXPECT_TRUE(argHelper->isGen(caseInsensitive));
            }
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
            std::transform(caseInsensitive.begin(), caseInsensitive.end(), caseInsensitive.begin(), ::tolower);
            EXPECT_EQ(argHelper->returnIGFXforGen(caseInsensitive), coreId);

            auto findCore = caseInsensitive.find("_core");
            if (findCore != std::string::npos) {
                caseInsensitive = caseInsensitive.substr(0, findCore);
                EXPECT_EQ(argHelper->returnIGFXforGen(caseInsensitive), coreId);
            }

            auto findUnderline = caseInsensitive.find("_");
            if (findUnderline != std::string::npos) {
                caseInsensitive.erase(std::remove(caseInsensitive.begin(), caseInsensitive.end(), '_'), caseInsensitive.end());
                EXPECT_EQ(argHelper->returnIGFXforGen(caseInsensitive), coreId);
            }
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

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenMutiplePlatformThenReturnThosePlatforms) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    if (allEnabledPlatforms.size() < 2) {
        GTEST_SKIP();
    }
    auto platform0 = allEnabledPlatforms[0];
    std::string platform0Name = NEO::hardwarePrefix[platform0];
    auto platform1 = allEnabledPlatforms[1];
    std::string platform1Name = NEO::hardwarePrefix[platform1];

    std::vector<ConstStringRef> expected{platform0Name, platform1Name};
    auto got = NEO::getTargetPlatformsForFatbinary(platform0Name + "," + platform1Name, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(expected, got);
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenPlatformOpenRangeFromThenReturnAllEnabledPlatformsThatMatch) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    if (allEnabledPlatforms.size() < 3) {
        GTEST_SKIP();
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

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenPlatformOpenRangeToThenReturnAllEnabledPlatformsThatMatch) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    if (allEnabledPlatforms.size() < 3) {
        GTEST_SKIP();
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

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenPlatformClosedRangeThenReturnAllEnabledPlatformsThatMatch) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    if (allEnabledPlatforms.size() < 4) {
        GTEST_SKIP();
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

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenArchitectureThenReturnAllEnabledConfigsThatMatch) {
    auto allEnabledCores = getEnabledCores();
    if (allEnabledCores.size() < 3) {
        GTEST_SKIP();
    }
    auto core = allEnabledCores[allEnabledCores.size() / 2];
    std::string coreName = NEO::familyName[core];
    if (coreName[0] == 'G') {
        coreName[0] = 'g';
    }

    std::vector<DeviceMapping> expected;
    oclocArgHelperWithoutInput->getProductConfigsForGfxCoreFamily(core, expected);
    auto got = NEO::getTargetConfigsForFatbinary(coreName, oclocArgHelperWithoutInput.get());

    EXPECT_EQ(expected.size(), got.size());
    for (unsigned int i = 0; i < got.size(); i++) {
        EXPECT_TRUE(expected[i] == got[i]);
    }
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenArchitectureOpenRangeFromThenReturnAllEnabledConfigsThatMatch) {
    auto allEnabledCores = getEnabledCores();
    if (allEnabledCores.size() < 3) {
        GTEST_SKIP();
    }
    auto core0 = allEnabledCores[allEnabledCores.size() / 2];
    std::string coreName = NEO::familyName[core0];
    if (coreName[0] == 'G') {
        coreName[0] = 'g';
    }

    std::vector<DeviceMapping> expected;
    unsigned int coreIt = core0;
    while (coreIt < static_cast<unsigned int>(IGFX_MAX_CORE)) {
        oclocArgHelperWithoutInput->getProductConfigsForGfxCoreFamily(static_cast<GFXCORE_FAMILY>(coreIt), expected);
        ++coreIt;
    }

    auto got = NEO::getTargetConfigsForFatbinary(coreName + "-", oclocArgHelperWithoutInput.get());
    EXPECT_EQ(expected.size(), got.size());
    for (unsigned int i = 0; i < got.size(); i++) {
        EXPECT_TRUE(expected[i] == got[i]);
    }
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenArchitectureOpenRangeToThenReturnAllEnabledConfigsThatMatch) {
    auto allEnabledCores = getEnabledCores();
    if (allEnabledCores.size() < 3) {
        GTEST_SKIP();
    }

    auto core0 = allEnabledCores[allEnabledCores.size() / 2];
    std::string coreName = NEO::familyName[core0];
    if (coreName[0] == 'G') {
        coreName[0] = 'g';
    }

    std::vector<DeviceMapping> expected;
    unsigned int coreIt = IGFX_UNKNOWN_CORE;
    ++coreIt;
    while (coreIt <= static_cast<unsigned int>(core0)) {
        oclocArgHelperWithoutInput->getProductConfigsForGfxCoreFamily(static_cast<GFXCORE_FAMILY>(coreIt), expected);
        ++coreIt;
    }

    auto got = NEO::getTargetConfigsForFatbinary("-" + coreName, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(expected.size(), got.size());
    for (unsigned int i = 0; i < got.size(); i++) {
        EXPECT_TRUE(expected[i] == got[i]);
    }
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenArchitectureClosedRangeThenReturnAllEnabledConfigsThatMatch) {
    auto allEnabledCores = getEnabledCores();
    if (allEnabledCores.size() < 4) {
        GTEST_SKIP();
    }
    auto coreFrom = allEnabledCores[1];
    auto coreTo = allEnabledCores[allEnabledCores.size() - 2];
    std::string coreNameFrom = NEO::familyName[coreFrom];
    if (coreNameFrom[0] == 'G') {
        coreNameFrom[0] = 'g';
    }
    std::string coreNameTo = NEO::familyName[coreTo];
    if (coreNameTo[0] == 'G') {
        coreNameTo[0] = 'g';
    }

    std::vector<DeviceMapping> expected;
    auto coreIt = coreFrom;
    while (coreIt <= coreTo) {
        oclocArgHelperWithoutInput->getProductConfigsForGfxCoreFamily(static_cast<GFXCORE_FAMILY>(coreIt), expected);
        coreIt = static_cast<GFXCORE_FAMILY>(static_cast<unsigned int>(coreIt) + 1);
    }

    auto got = NEO::getTargetConfigsForFatbinary(coreNameFrom + "-" + coreNameTo, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(expected.size(), got.size());
    for (unsigned int i = 0; i < got.size(); i++) {
        EXPECT_TRUE(expected[i] == got[i]);
    }

    got = NEO::getTargetConfigsForFatbinary(coreNameTo + "-" + coreNameFrom, oclocArgHelperWithoutInput.get()); // swap min with max implicitly
    EXPECT_EQ(expected.size(), got.size());
    for (unsigned int i = 0; i < got.size(); i++) {
        EXPECT_TRUE(expected[i] == got[i]);
    }
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenUnkownArchitectureThenReturnEmptyList) {
    auto got = NEO::getTargetConfigsForFatbinary("gen0", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenMutiplePlatformWhenSecondPlatformsIsUnknownThenReturnErrorMessage) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    auto platform0 = allEnabledPlatforms[0];
    std::string platform0Name = NEO::hardwarePrefix[platform0];

    auto platformTarget = platform0Name + ",unk";

    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        platformTarget};

    testing::internal::CaptureStdout();
    int retVal = buildFatBinary(argv, argHelper.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_NE(retVal, NEO::OclocErrorCode::SUCCESS);

    resString << "Unknown device : unk\n";
    resString << "Failed to parse target devices from : " << platformTarget << "\n";
    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenClosedRangeTooExtensiveWhenConfigIsValidThenErrorMessageAndFailIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();
    if (allEnabledDeviceConfigs.size() < 4) {
        GTEST_SKIP();
    }
    std::string configNum0 = argHelper->parseProductConfigFromValue(allEnabledDeviceConfigs[0].config);
    std::string configNum1 = argHelper->parseProductConfigFromValue(allEnabledDeviceConfigs[1].config);
    std::string configNum2 = argHelper->parseProductConfigFromValue(allEnabledDeviceConfigs[2].config);

    std::stringstream configString;
    configString << configNum0 << "-" << configNum1 << "-" << configNum2;

    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        configString.str()};

    testing::internal::CaptureStdout();
    int retVal = buildFatBinary(argv, argHelper.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_NE(retVal, NEO::OclocErrorCode::SUCCESS);
    resString << "Invalid range : " << configString.str() << " - should be from-to or -to or from-"
              << "\n";
    resString << "Failed to parse target devices from : " << configString.str() << "\n";
    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenClosedRangeTooExtensiveWhenPlatformIsValidThenErrorMessageAndReturnEmptyList) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    if (allEnabledPlatforms.size() < 4) {
        GTEST_SKIP();
    }
    auto platform0 = allEnabledPlatforms[0];
    std::string platform0Name = NEO::hardwarePrefix[platform0];
    auto platform1 = allEnabledPlatforms[1];
    std::string platform1Name = NEO::hardwarePrefix[platform1];
    auto platform2 = allEnabledPlatforms[2];
    std::string platform2Name = NEO::hardwarePrefix[platform2];
    std::string platformsTarget = platform0Name + "-" + platform1Name + "-" + platform2Name;

    std::string resString = "Invalid range : " + platformsTarget + " - should be from-to or -to or from-\n";

    testing::internal::CaptureStdout();
    auto got = NEO::getTargetPlatformsForFatbinary(platformsTarget, argHelper.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ(output.c_str(), resString.c_str());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenPlatformClosedRangeWhenSecondPlatformIsUnkownThenReturnEmptyList) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    auto platform0 = allEnabledPlatforms[0];
    std::string platform0Name = NEO::hardwarePrefix[platform0];

    auto got = NEO::getTargetPlatformsForFatbinary(platform0Name + "-unk", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenGenOpenRangeFromWhenGenIsUnknownThenReturnEmptyList) {
    auto got = NEO::getTargetConfigsForFatbinary("gen2-", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenGenOpenRangeToWhenGenIsUnknownThenReturnEmptyList) {
    auto got = NEO::getTargetConfigsForFatbinary("-gen2", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenGenClosedRangeWhenAnyOfGensIsUnknownThenReturnEmptyList) {
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    auto platform0 = allEnabledPlatforms[0];
    auto gfxCore0 = NEO::hardwareInfoTable[platform0]->platform.eRenderCoreFamily;
    std::string genName = NEO::familyName[gfxCore0];
    if (genName[0] == 'G') {
        genName[0] = 'g';
    }

    auto got = NEO::getTargetConfigsForFatbinary("gen2-" + genName, oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetConfigsForFatbinary(genName + "-gen2", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetConfigsForFatbinary(genName + ",gen2", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetConfigsForFatbinary("gen2," + genName, oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenTwoPlatformsWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    if (allEnabledPlatforms.size() < 3) {
        GTEST_SKIP();
    }

    auto platform0 = allEnabledPlatforms[0];
    ConstStringRef platformName0(hardwarePrefix[platform0], strlen(hardwarePrefix[platform0]));
    auto platform1 = allEnabledPlatforms[1];
    ConstStringRef platformName1(hardwarePrefix[platform1], strlen(hardwarePrefix[platform1]));

    std::vector<ConstStringRef> expected{platformName0, platformName1};

    std::string platformsTarget = platformName0.str() + "," + platformName1.str();

    auto got = NEO::getTargetPlatformsForFatbinary(platformsTarget, argHelper.get());
    EXPECT_EQ(expected, got);

    auto platformRev0 = std::to_string(hardwareInfoTable[platform0]->platform.usRevId);
    auto platformRev1 = std::to_string(hardwareInfoTable[platform1]->platform.usRevId);
    std::vector<std::string> platformsRevision{platformRev0, platformRev1};
    std::stringstream resString;

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        platformsTarget};

    testing::internal::CaptureStdout();
    int retVal = buildFatBinary(argv, argHelper.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

    for (uint32_t i = 0; i < got.size(); i++) {
        resString << "Build succeeded for : " << expected[i].str() + "." + platformsRevision[i] + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenPlatformsClosedRangeWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    if (allEnabledPlatforms.size() < 4) {
        GTEST_SKIP();
    }

    auto platformFrom = allEnabledPlatforms[0];
    ConstStringRef platformNameFrom(hardwarePrefix[platformFrom], strlen(hardwarePrefix[platformFrom]));
    auto platformTo = allEnabledPlatforms[allEnabledPlatforms.size() / 2];
    ConstStringRef platformNameTo(hardwarePrefix[platformTo], strlen(hardwarePrefix[platformTo]));

    if (platformFrom > platformTo) {
        std::swap(platformFrom, platformTo);
    }

    std::vector<PRODUCT_FAMILY> requestedPlatforms;
    auto from = std::find(allEnabledPlatforms.begin(), allEnabledPlatforms.end(), platformFrom);
    auto to = std::find(allEnabledPlatforms.begin(), allEnabledPlatforms.end(), platformTo) + 1;
    requestedPlatforms.insert(requestedPlatforms.end(), from, to);

    auto expected = toProductNames(requestedPlatforms);

    std::string platformsTarget = platformNameFrom.str() + "-" + platformNameTo.str();

    auto got = NEO::getTargetPlatformsForFatbinary(platformsTarget, argHelper.get());
    EXPECT_EQ(expected, got);

    std::vector<std::string> platformsRevisions;

    for (auto platform : requestedPlatforms) {
        platformsRevisions.push_back(std::to_string(hardwareInfoTable[platform]->platform.usRevId));
    }

    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        platformsTarget};

    testing::internal::CaptureStdout();
    int retVal = buildFatBinary(argv, argHelper.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

    for (uint32_t i = 0; i < got.size(); i++) {
        resString << "Build succeeded for : " << expected[i].str() + "." + platformsRevisions[i] + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenPlatformsOpenRangeToWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    if (allEnabledPlatforms.size() < 4) {
        GTEST_SKIP();
    }

    auto platformTo = allEnabledPlatforms[0];
    ConstStringRef platformNameTo(hardwarePrefix[platformTo], strlen(hardwarePrefix[platformTo]));

    std::vector<PRODUCT_FAMILY> requestedPlatforms;
    auto platformToId = std::find(allEnabledPlatforms.begin(), allEnabledPlatforms.end(), platformTo);
    assert(platformToId != allEnabledPlatforms.end());

    requestedPlatforms.insert(requestedPlatforms.end(), allEnabledPlatforms.begin(), platformToId + 1);

    auto expected = toProductNames(requestedPlatforms);

    std::string platformsTarget = "-" + platformNameTo.str();

    auto got = NEO::getTargetPlatformsForFatbinary(platformsTarget, argHelper.get());
    EXPECT_EQ(expected, got);

    std::vector<std::string> platformsRevisions;

    for (auto platform : requestedPlatforms) {
        platformsRevisions.push_back(std::to_string(hardwareInfoTable[platform]->platform.usRevId));
    }

    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        platformsTarget};

    testing::internal::CaptureStdout();
    int retVal = buildFatBinary(argv, argHelper.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

    for (uint32_t i = 0; i < got.size(); i++) {
        resString << "Build succeeded for : " << expected[i].str() + "." + platformsRevisions[i] + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenPlatformsOpenRangeFromWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledPlatforms = NEO::getAllSupportedTargetPlatforms();
    if (allEnabledPlatforms.size() < 4) {
        GTEST_SKIP();
    }

    auto platformFrom = allEnabledPlatforms[0];
    ConstStringRef platformNameFrom(hardwarePrefix[platformFrom], strlen(hardwarePrefix[platformFrom]));

    std::vector<PRODUCT_FAMILY> requestedPlatforms;
    auto platformToId = std::find(allEnabledPlatforms.begin(), allEnabledPlatforms.end(), platformFrom);
    assert(platformToId != allEnabledPlatforms.end());

    requestedPlatforms.insert(requestedPlatforms.end(), platformToId, allEnabledPlatforms.end());

    auto expected = toProductNames(requestedPlatforms);

    std::string platformsTarget = platformNameFrom.str() + "-";

    auto got = NEO::getTargetPlatformsForFatbinary(platformsTarget, argHelper.get());
    EXPECT_EQ(expected, got);

    std::vector<std::string> platformsRevisions;

    for (auto platform : requestedPlatforms) {
        platformsRevisions.push_back(std::to_string(hardwareInfoTable[platform]->platform.usRevId));
    }

    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        platformsTarget};

    testing::internal::CaptureStdout();
    int retVal = buildFatBinary(argv, argHelper.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

    for (uint32_t i = 0; i < got.size(); i++) {
        resString << "Build succeeded for : " << expected[i].str() + "." + platformsRevisions[i] + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenTwoConfigsWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();
    if (allEnabledDeviceConfigs.size() < 2) {
        GTEST_SKIP();
    }
    auto config0 = allEnabledDeviceConfigs[0];
    auto config1 = allEnabledDeviceConfigs[1];

    auto configStr0 = argHelper->parseProductConfigFromValue(config0.config);
    auto configStr1 = argHelper->parseProductConfigFromValue(config1.config);

    std::vector<std::string> targets{configStr0, configStr1};
    std::vector<DeviceMapping> expected;

    for (auto &target : targets) {
        auto configFirstEl = argHelper->findConfigMatch(target, true);

        auto configLastEl = argHelper->findConfigMatch(target, false);
        for (auto &deviceConfig : allEnabledDeviceConfigs) {
            if (deviceConfig.config >= configFirstEl && deviceConfig.config <= configLastEl) {
                expected.push_back(deviceConfig);
            }
        }
    }

    auto configsTarget = configStr0 + "," + configStr1;
    auto got = NEO::getTargetConfigsForFatbinary(configsTarget, argHelper.get());

    EXPECT_EQ(expected.size(), got.size());
    for (unsigned int i = 0; i < got.size(); i++) {
        EXPECT_TRUE(expected[i] == got[i]);
    }

    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        configsTarget};

    testing::internal::CaptureStdout();
    int retVal = buildFatBinary(argv, argHelper.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

    for (auto deviceConfig : expected) {
        auto targetConfig = argHelper->parseProductConfigFromValue(deviceConfig.config);
        resString << "Build succeeded for : " << targetConfig + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenProductConfigOpenRangeFromWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();
    if (allEnabledDeviceConfigs.size() < 2) {
        GTEST_SKIP();
    }
    auto deviceMapConfig = allEnabledDeviceConfigs[allEnabledDeviceConfigs.size() / 2];
    auto configNumConvention = argHelper->parseProductConfigFromValue(deviceMapConfig.config);

    std::vector<DeviceMapping> expected;
    auto configFrom = std::find_if(allEnabledDeviceConfigs.begin(),
                                   allEnabledDeviceConfigs.end(),
                                   [&cf = deviceMapConfig](const DeviceMapping &c) -> bool { return cf.config == c.config; });

    expected.insert(expected.end(), configFrom, allEnabledDeviceConfigs.end());

    auto configsTarget = configNumConvention + "-";
    auto got = NEO::getTargetConfigsForFatbinary(configsTarget, argHelper.get());

    EXPECT_EQ(expected.size(), got.size());
    for (unsigned int i = 0; i < got.size(); i++) {
        EXPECT_TRUE(expected[i] == got[i]);
    }

    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        configsTarget};

    testing::internal::CaptureStdout();
    int retVal = buildFatBinary(argv, argHelper.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

    for (auto deviceConfig : expected) {
        auto targetConfig = argHelper->parseProductConfigFromValue(deviceConfig.config);
        resString << "Build succeeded for : " << targetConfig + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenProductConfigOpenRangeToWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();
    if (allEnabledDeviceConfigs.size() < 2) {
        GTEST_SKIP();
    }

    auto deviceMapConfig = allEnabledDeviceConfigs[allEnabledDeviceConfigs.size() / 2];
    auto configNumConvention = argHelper->parseProductConfigFromValue(deviceMapConfig.config);

    std::vector<DeviceMapping> expected;
    for (auto &deviceConfig : allEnabledDeviceConfigs) {
        if (deviceConfig.config <= deviceMapConfig.config) {
            expected.push_back(deviceConfig);
        }
    }
    auto configsTarget = "-" + configNumConvention;
    auto got = NEO::getTargetConfigsForFatbinary(configsTarget, argHelper.get());

    EXPECT_EQ(expected.size(), got.size());
    for (unsigned int i = 0; i < got.size(); i++) {
        EXPECT_TRUE(expected[i] == got[i]);
    }

    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        configsTarget};

    testing::internal::CaptureStdout();
    int retVal = buildFatBinary(argv, argHelper.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

    for (auto deviceConfig : expected) {
        auto targetConfig = argHelper->parseProductConfigFromValue(deviceConfig.config);
        resString << "Build succeeded for : " << targetConfig + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenProductConfigClosedRangeWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();
    if (allEnabledDeviceConfigs.size() < 4) {
        GTEST_SKIP();
    }

    auto deviceMapConfigFrom = allEnabledDeviceConfigs[1];
    auto deviceMapConfigTo = allEnabledDeviceConfigs[allEnabledDeviceConfigs.size() - 2];
    auto configFromNumConvention = argHelper->parseProductConfigFromValue(deviceMapConfigFrom.config);
    auto configToNumConvention = argHelper->parseProductConfigFromValue(deviceMapConfigTo.config);

    std::vector<DeviceMapping> expected;

    for (auto &deviceConfig : allEnabledDeviceConfigs) {
        if (deviceConfig.config >= deviceMapConfigFrom.config && deviceConfig.config <= deviceMapConfigTo.config) {
            expected.push_back(deviceConfig);
        }
    }
    auto configsTarget = configFromNumConvention + "-" + configToNumConvention;
    auto got = NEO::getTargetConfigsForFatbinary(configsTarget, argHelper.get()); // swap min with max implicitly
    EXPECT_EQ(expected.size(), got.size());
    for (unsigned int i = 0; i < got.size(); i++) {
        EXPECT_TRUE(expected[i] == got[i]);
    }
    got = NEO::getTargetConfigsForFatbinary(configToNumConvention + "-" + configFromNumConvention, argHelper.get()); // swap min with max implicitly

    EXPECT_EQ(expected.size(), got.size());
    for (unsigned int i = 0; i < got.size(); i++) {
        EXPECT_TRUE(expected[i] == got[i]);
    }

    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        configsTarget};

    testing::internal::CaptureStdout();
    int retVal = buildFatBinary(argv, argHelper.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

    for (auto deviceConfig : expected) {
        auto targetConfig = argHelper->parseProductConfigFromValue(deviceConfig.config);
        resString << "Build succeeded for : " << targetConfig + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenArgsWhenCorrectDeviceNumerationIsProvidedWithoutRevisionThenTargetsAreFound) {
    auto allEnabledDeviceConfigs = oclocArgHelperWithoutInput->getAllSupportedDeviceConfigs();
    if (allEnabledDeviceConfigs.size() < 2) {
        GTEST_SKIP();
    }

    std::string configNum0 = oclocArgHelperWithoutInput->parseProductConfigFromValue(allEnabledDeviceConfigs[0].config);
    auto major_pos = configNum0.find(".");
    auto minor_pos = configNum0.find(".", ++major_pos);
    auto cutRevision = configNum0.substr(0, minor_pos);

    auto got = NEO::getTargetConfigsForFatbinary(ConstStringRef(cutRevision), oclocArgHelperWithoutInput.get());
    EXPECT_FALSE(got.empty());
}

TEST_F(OclocFatBinaryGetTargetConfigsForFatbinary, GivenArgsWhenCorrectDeviceNumerationIsProvidedWithoutMinorAndRevisionThenTargetsAreFound) {
    auto allEnabledDeviceConfigs = oclocArgHelperWithoutInput->getAllSupportedDeviceConfigs();
    if (allEnabledDeviceConfigs.size() < 2) {
        GTEST_SKIP();
    }

    std::string configNum0 = oclocArgHelperWithoutInput->parseProductConfigFromValue(allEnabledDeviceConfigs[0].config);
    auto major_pos = configNum0.find(".");
    auto cutMinorAndRevision = configNum0.substr(0, major_pos);

    auto got = NEO::getTargetConfigsForFatbinary(ConstStringRef(cutMinorAndRevision), oclocArgHelperWithoutInput.get());
    EXPECT_FALSE(got.empty());
}

TEST_F(OclocFatBinaryTest, GivenSpirvInputWhenFatBinaryIsRequestedThenArchiveContainsGenericIrFileWithSpirvContent) {
    const auto devices = prepareTwoDevices(&mockArgHelper);
    if (devices.empty()) {
        GTEST_SKIP();
    }

    const std::vector<std::string> args = {
        "ocloc",
        "-output",
        outputArchiveName,
        "-file",
        spirvFilename,
        "-output_no_suffix",
        "-spirv_input",
        "-device",
        devices};

    const auto buildResult = buildFatBinary(args, &mockArgHelper);
    ASSERT_EQ(OclocErrorCode::SUCCESS, buildResult);
    ASSERT_EQ(1u, mockArgHelper.interceptedFiles.count(outputArchiveName));

    const auto &rawArchive = mockArgHelper.interceptedFiles[outputArchiveName];
    const auto archiveBytes = ArrayRef<const std::uint8_t>::fromAny(rawArchive.data(), rawArchive.size());

    std::string outErrReason{};
    std::string outWarning{};
    const auto decodedArchive = NEO::Ar::decodeAr(archiveBytes, outErrReason, outWarning);

    ASSERT_NE(nullptr, decodedArchive.magic);
    ASSERT_TRUE(outErrReason.empty());
    ASSERT_TRUE(outWarning.empty());

    const auto spirvFileIt = searchInArchiveByFilename(decodedArchive, archiveGenericIrName);
    ASSERT_NE(decodedArchive.files.end(), spirvFileIt);

    const auto elf = Elf::decodeElf<Elf::EI_CLASS_64>(spirvFileIt->fileData, outErrReason, outWarning);
    ASSERT_NE(nullptr, elf.elfFileHeader);
    ASSERT_TRUE(outErrReason.empty());
    ASSERT_TRUE(outWarning.empty());

    const auto isSpirvSection = [](const auto &section) {
        return section.header && section.header->type == Elf::SHT_OPENCL_SPIRV;
    };

    const auto spirvSectionIt = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(), isSpirvSection);
    ASSERT_NE(elf.sectionHeaders.end(), spirvSectionIt);

    ASSERT_EQ(spirvFileContent.size() + 1, spirvSectionIt->header->size);
    const auto isSpirvDataEqualsInputFileData = std::memcmp(spirvFileContent.data(), spirvSectionIt->data.begin(), spirvFileContent.size()) == 0;
    EXPECT_TRUE(isSpirvDataEqualsInputFileData);
}

TEST_F(OclocFatBinaryTest, GivenSpirvInputAndExcludeIrFlagWhenFatBinaryIsRequestedThenArchiveDoesNotContainGenericIrFile) {
    const auto devices = prepareTwoDevices(&mockArgHelper);
    if (devices.empty()) {
        GTEST_SKIP();
    }

    const std::vector<std::string> args = {
        "ocloc",
        "-output",
        outputArchiveName,
        "-file",
        spirvFilename,
        "-output_no_suffix",
        "-spirv_input",
        "-exclude_ir",
        "-device",
        devices};

    const auto buildResult = buildFatBinary(args, &mockArgHelper);
    ASSERT_EQ(OclocErrorCode::SUCCESS, buildResult);
    ASSERT_EQ(1u, mockArgHelper.interceptedFiles.count(outputArchiveName));

    const auto &rawArchive = mockArgHelper.interceptedFiles[outputArchiveName];
    const auto archiveBytes = ArrayRef<const std::uint8_t>::fromAny(rawArchive.data(), rawArchive.size());

    std::string outErrReason{};
    std::string outWarning{};
    const auto decodedArchive = NEO::Ar::decodeAr(archiveBytes, outErrReason, outWarning);

    ASSERT_NE(nullptr, decodedArchive.magic);
    ASSERT_TRUE(outErrReason.empty());
    ASSERT_TRUE(outWarning.empty());

    const auto spirvFileIt = searchInArchiveByFilename(decodedArchive, archiveGenericIrName);
    EXPECT_EQ(decodedArchive.files.end(), spirvFileIt);
}

TEST_F(OclocFatBinaryTest, GivenClInputFileWhenFatBinaryIsRequestedThenArchiveDoesNotContainGenericIrFile) {
    const auto devices = prepareTwoDevices(&mockArgHelper);
    if (devices.empty()) {
        GTEST_SKIP();
    }

    const std::string clFilename = "some_kernel.cl";
    mockArgHelperFilesMap[clFilename] = "__kernel void some_kernel(){}";

    const std::vector<std::string> args = {
        "ocloc",
        "-output",
        outputArchiveName,
        "-file",
        clFilename,
        "-output_no_suffix",
        "-device",
        devices};

    const auto buildResult = buildFatBinary(args, &mockArgHelper);
    ASSERT_EQ(OclocErrorCode::SUCCESS, buildResult);
    ASSERT_EQ(1u, mockArgHelper.interceptedFiles.count(outputArchiveName));

    const auto &rawArchive = mockArgHelper.interceptedFiles[outputArchiveName];
    const auto archiveBytes = ArrayRef<const std::uint8_t>::fromAny(rawArchive.data(), rawArchive.size());

    std::string outErrReason{};
    std::string outWarning{};
    const auto decodedArchive = NEO::Ar::decodeAr(archiveBytes, outErrReason, outWarning);

    ASSERT_NE(nullptr, decodedArchive.magic);
    ASSERT_TRUE(outErrReason.empty());
    ASSERT_TRUE(outWarning.empty());

    const auto spirvFileIt = searchInArchiveByFilename(decodedArchive, archiveGenericIrName);
    EXPECT_EQ(decodedArchive.files.end(), spirvFileIt);
}

TEST_F(OclocFatBinaryTest, GivenEmptyFileWhenAppendingGenericIrThenInvalidFileIsReturned) {
    Ar::ArEncoder ar;
    std::string emptyFile{"empty_file.spv"};
    mockArgHelperFilesMap[emptyFile] = "";
    mockArgHelper.shouldLoadDataFromFileReturnZeroSize = true;

    ::testing::internal::CaptureStdout();
    const auto errorCode{appendGenericIr(ar, emptyFile, &mockArgHelper)};
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::INVALID_FILE, errorCode);
    EXPECT_EQ("Error! Couldn't read input file!\n", output);
}

TEST_F(OclocFatBinaryTest, GivenInvalidIrFileWhenAppendingGenericIrThenInvalidFileIsReturned) {
    Ar::ArEncoder ar;
    std::string dummyFile{"dummy_file.spv"};
    mockArgHelperFilesMap[dummyFile] = "This is not IR!";

    ::testing::internal::CaptureStdout();
    const auto errorCode{appendGenericIr(ar, dummyFile, &mockArgHelper)};
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::INVALID_FILE, errorCode);

    const auto expectedErrorMessage{"Error! Input file is not in supported generic IR format! "
                                    "Currently supported format is SPIR-V.\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

} // namespace NEO