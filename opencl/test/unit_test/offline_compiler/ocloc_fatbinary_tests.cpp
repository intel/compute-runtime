/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/ocloc_fatbinary_tests.h"

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/source/compiler_interface/compiler_options/compiler_options_base.h"
#include "shared/source/device_binary_format/ar/ar.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/product_config_helper.h"

#include "environment.h"
#include "mock/mock_argument_helper.h"
#include "mock/mock_offline_compiler.h"
#include "platforms.h"

#include <algorithm>
#include <unordered_set>

extern Environment *gEnvironment;

namespace NEO {

auto searchInArchiveByFilename(const Ar::Ar &archive, const ConstStringRef &name) {
    const auto isSearchedFile = [&name](const auto &file) {
        return file.fileName == name;
    };

    const auto &arFiles = archive.files;
    return std::find_if(arFiles.begin(), arFiles.end(), isSearchedFile);
}

auto allFilesInArchiveExceptPaddingStartsWith(const Ar::Ar &archive, const ConstStringRef &prefix) {
    const auto &arFiles = archive.files;
    for (const auto &file : arFiles) {
        if (file.fileName.startsWith("pad")) {
            continue;
        }

        if (!file.fileName.startsWith(prefix.data())) {
            return false;
        }
    }

    return true;
}

std::string getDeviceConfig(const OfflineCompiler &offlineCompiler, MockOclocArgHelper *argHelper) {
    auto allEnabledDeviceConfigs = argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        return {};
    }

    const auto &hwInfo = offlineCompiler.getHardwareInfo();
    for (const auto &device : allEnabledDeviceConfigs) {
        if (device.hwInfo->platform.eProductFamily == hwInfo.platform.eProductFamily) {
            return ProductConfigHelper::parseMajorMinorRevisionValue(device.aotConfig);
        }
    }
    return {};
}

std::string prepareTwoDevices(MockOclocArgHelper *argHelper) {
    auto enabledProductsAcronyms = argHelper->productConfigHelper->getRepresentativeProductAcronyms();
    if (enabledProductsAcronyms.size() < 2) {
        return {};
    }

    const auto cfg1 = enabledProductsAcronyms[0].str();
    const auto cfg2 = enabledProductsAcronyms[1].str();
    return cfg1 + "," + cfg2;
}

void appendAcronymWithoutDashes(std::vector<std::string> &out, ConstStringRef acronym) {
    if (acronym.contains("-")) {
        auto acronymCopy = acronym.str();
        auto findDash = acronymCopy.find("-");
        if (findDash == std::string::npos) {
            acronymCopy.erase(std::remove(acronymCopy.begin(), acronymCopy.end(), '-'), acronymCopy.end());
        }
        out.push_back(acronymCopy);
    }
}

std::vector<std::string> prepareProductsWithoutDashes(OclocArgHelper *argHelper) {
    auto enabledProductsAcronyms = argHelper->productConfigHelper->getRepresentativeProductAcronyms();
    if (enabledProductsAcronyms.size() < 2) {
        return {};
    }
    std::vector<std::string> acronyms{};
    for (const auto &acronym : enabledProductsAcronyms) {
        appendAcronymWithoutDashes(acronyms, acronym);
        if (acronyms.size() > 1)
            break;
    }
    return acronyms;
}

std::vector<std::string> prepareReleasesWithoutDashes(OclocArgHelper *argHelper) {
    auto enabledReleasesAcronyms = argHelper->productConfigHelper->getReleasesAcronyms();
    if (enabledReleasesAcronyms.size() < 2) {
        return {};
    }
    std::vector<std::string> acronyms{};
    for (const auto &acronym : enabledReleasesAcronyms) {
        appendAcronymWithoutDashes(acronyms, acronym);
        if (acronyms.size() > 1)
            break;
    }
    return acronyms;
}

std::vector<std::string> prepareFamiliesWithoutDashes(OclocArgHelper *argHelper) {
    auto enabledFamiliesAcronyms = argHelper->productConfigHelper->getFamiliesAcronyms();
    if (enabledFamiliesAcronyms.size() < 2) {
        return {};
    }
    std::vector<std::string> acronyms{};
    for (const auto &acronym : enabledFamiliesAcronyms) {
        appendAcronymWithoutDashes(acronyms, acronym);
        if (acronyms.size() > 1)
            break;
    }
    return acronyms;
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

TEST(OclocFatBinaryRequestedFatBinary, givenHwInfoForProductConfigWhenUnknownIsaIsPassedThenFalseIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    NEO::HardwareInfo hwInfo;
    EXPECT_FALSE(argHelper->getHwInfoForProductConfig(AOT::UNKNOWN_ISA, hwInfo, 0u));
}

TEST(OclocFatBinaryRequestedFatBinary, givenReleaseOrFamilyAcronymWhenGetAcronymsForTargetThenCorrectValuesAreReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto &enabledDeviceConfigs = argHelper->productConfigHelper->getDeviceAotInfo();
    if (enabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    ConstStringRef acronym("");
    std::vector<ConstStringRef> outRelease{}, outFamily{};

    for (auto &device : enabledDeviceConfigs) {
        if (!device.acronyms.empty()) {
            acronym = device.acronyms.front();
            getProductsAcronymsForTarget<AOT::RELEASE>(outRelease, device.release, argHelper.get());
            EXPECT_TRUE(std::find(outRelease.begin(), outRelease.end(), acronym) != outRelease.end());

            getProductsAcronymsForTarget<AOT::FAMILY>(outFamily, device.family, argHelper.get());
            EXPECT_TRUE(std::find(outFamily.begin(), outFamily.end(), acronym) != outFamily.end());

            device.acronyms.clear();
            outRelease.clear();
            outFamily.clear();

            getProductsAcronymsForTarget<AOT::RELEASE>(outRelease, device.release, argHelper.get());
            EXPECT_FALSE(std::find(outRelease.begin(), outRelease.end(), acronym) != outRelease.end());

            getProductsAcronymsForTarget<AOT::FAMILY>(outFamily, device.family, argHelper.get());
            EXPECT_FALSE(std::find(outFamily.begin(), outFamily.end(), acronym) != outFamily.end());
        }
    }
}

TEST(OclocFatBinaryRequestedFatBinary, givenDeviceArgToFatBinaryWhenConfigIsNotFullThenFalseIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allEnabledDeviceConfigs = argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }
    auto aotConfig = allEnabledDeviceConfigs[0].aotConfig;

    std::stringstream majorString;
    majorString << aotConfig.ProductConfigID.Major;
    auto major = majorString.str();

    auto aotValue0 = argHelper->productConfigHelper->getProductConfigForVersionValue(major);
    EXPECT_EQ(aotValue0, AOT::UNKNOWN_ISA);

    auto majorMinor = ProductConfigHelper::parseMajorMinorValue(aotConfig);
    auto aotValue1 = argHelper->productConfigHelper->getProductConfigForVersionValue(majorMinor);
    EXPECT_EQ(aotValue1, AOT::UNKNOWN_ISA);

    const char *cutRevision[] = {"ocloc", "-device", majorMinor.c_str()};
    const char *cutMinorAndRevision[] = {"ocloc", "-device", major.c_str()};

    EXPECT_FALSE(NEO::requestedFatBinary(3, cutRevision, argHelper.get()));
    EXPECT_FALSE(NEO::requestedFatBinary(3, cutMinorAndRevision, argHelper.get()));
}

TEST(OclocFatBinaryRequestedFatBinary, givenDeviceArgProvidedWhenFatBinaryFormatWithRangeIsPassedThenTrueIsReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();

    const char *allPlatforms[] = {"ocloc", "-device", "*"};
    const char *manyPlatforms[] = {"ocloc", "-device", "a,b"};
    const char *manyGens[] = {"ocloc", "-device", "family0,family1"};
    const char *rangePlatformFrom[] = {"ocloc", "-device", "skl:"};
    const char *rangePlatformTo[] = {"ocloc", "-device", ":skl"};
    const char *rangePlatformBounds[] = {"ocloc", "-device", "skl:icllp"};
    const char *rangeGenFrom[] = {"ocloc", "-device", "family0:"};
    const char *rangeGenTo[] = {"ocloc", "-device", ":release5"};
    const char *rangeGenBounds[] = {"ocloc", "-device", "family0:family5"};

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

TEST_F(OclocFatBinaryProductAcronymsTests, givenDeviceArgAsSingleProductThenFatBinaryIsNotRequested) {
    auto allEnabledDeviceConfigs = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();

    for (const auto &deviceConfig : allEnabledDeviceConfigs) {
        std::string configStr = ProductConfigHelper::parseMajorMinorRevisionValue(deviceConfig.aotConfig);
        const char *singleConfig[] = {"ocloc", "-device", configStr.c_str()};
        EXPECT_FALSE(NEO::requestedFatBinary(3, singleConfig, oclocArgHelperWithoutInput.get()));

        for (const auto &acronym : deviceConfig.acronyms) {
            auto acronymStr = acronym.str();
            const char *singleAcronym[] = {"ocloc", "-device", acronymStr.c_str()};
            EXPECT_FALSE(NEO::requestedFatBinary(3, singleAcronym, oclocArgHelperWithoutInput.get()));
        }
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenAsterixThenReturnAllEnabledAcronyms) {
    auto expected = oclocArgHelperWithoutInput->productConfigHelper->getRepresentativeProductAcronyms();
    auto got = NEO::getTargetProductsForFatbinary("*", oclocArgHelperWithoutInput.get());

    EXPECT_EQ(got.size(), expected.size());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenDeviceArgProvidedWhenUnknownFamilyNameIsPassedThenRequestedFatBinaryReturnsFalse) {
    const char *unknownFamily[] = {"ocloc", "-device", "gen0"};
    EXPECT_FALSE(NEO::requestedFatBinary(3, unknownFamily, oclocArgHelperWithoutInput.get()));
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenDeviceArgProvidedWhenKnownNameIsPassedWithCoreSuffixThenRequestedFatBinaryReturnsTrue) {
    for (const auto *acronyms : {&enabledFamiliesAcronyms, &enabledReleasesAcronyms}) {
        for (const auto &acronym : *acronyms) {
            auto acronymStr = acronym.str() + "_core";
            const char *name[] = {"ocloc", "-device", acronymStr.c_str()};
            EXPECT_TRUE(NEO::requestedFatBinary(3, name, oclocArgHelperWithoutInput.get()));
        }
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenDeviceArgProvidedWhenKnownNameIsPassedThenRequestedFatBinaryReturnsTrue) {
    for (const auto *acronyms : {&enabledFamiliesAcronyms, &enabledReleasesAcronyms}) {
        for (const auto &acronym : *acronyms) {
            auto acronymStr = acronym.str();
            const char *name[] = {"ocloc", "-device", acronymStr.c_str()};
            EXPECT_TRUE(NEO::requestedFatBinary(3, name, oclocArgHelperWithoutInput.get()));
        }
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenUnkownArchitectureThenReturnEmptyList) {
    auto got = NEO::getTargetProductsForFatbinary("gen0", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenClosedRangeTooExtensiveWhenProductsOrFamiliesOrReleasesAreValidThenFailIsReturned) {
    for (const auto *enabledAcronymsPtr : {&enabledProductsAcronyms, &enabledReleasesAcronyms, &enabledFamiliesAcronyms}) {
        const auto &enabledAcronyms = *enabledAcronymsPtr;
        if (enabledAcronyms.size() < 3) {
            GTEST_SKIP();
        }

        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
        std::stringstream acronymsString;
        acronymsString << enabledAcronyms[0].str() << ":" << enabledAcronyms[1].str() << ":" << enabledAcronyms[2].str();
        auto target = acronymsString.str();

        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            target};

        testing::internal::CaptureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = testing::internal::GetCapturedStdout();
        EXPECT_NE(retVal, NEO::OclocErrorCode::SUCCESS);
        resString << "Invalid range : " << acronymsString.str() << " - should be from:to or :to or from:\n";
        resString << "Failed to parse target devices from : " << target << "\n";
        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenAcronymOpenRangeWhenAcronymIsUnknownThenReturnEmptyList) {
    auto got = NEO::getTargetProductsForFatbinary("unk:", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());

    got = NEO::getTargetProductsForFatbinary(":unk", oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(got.empty());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenClosedRangeWhenAnyOfAcronymIsUnknownThenReturnEmptyList) {
    for (const auto *vec : {&enabledProductsAcronyms, &enabledReleasesAcronyms, &enabledFamiliesAcronyms}) {
        for (const auto &acronym : *vec) {
            auto got = NEO::getTargetProductsForFatbinary("unk:" + acronym.str(), oclocArgHelperWithoutInput.get());
            EXPECT_TRUE(got.empty());

            got = NEO::getTargetProductsForFatbinary(acronym.str() + ":unk", oclocArgHelperWithoutInput.get());
            EXPECT_TRUE(got.empty());

            got = NEO::getTargetProductsForFatbinary(acronym.str() + ",unk", oclocArgHelperWithoutInput.get());
            EXPECT_TRUE(got.empty());

            got = NEO::getTargetProductsForFatbinary("unk," + acronym.str(), oclocArgHelperWithoutInput.get());
            EXPECT_TRUE(got.empty());
        }
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenTwoTargetsOfProductsWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledProductsAcronyms.size() < 2) {
        GTEST_SKIP();
    }
    for (unsigned int product = 0; product < enabledProductsAcronyms.size() - 1; product++) {
        auto acronym0 = enabledProductsAcronyms.at(product);
        auto acronym1 = enabledProductsAcronyms.at(product + 1);
        std::vector<ConstStringRef> expected{acronym0, acronym1};

        std::string acronymsTarget = acronym0.str() + "," + acronym1.str();
        auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(got, expected);

        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            acronymsTarget};

        testing::internal::CaptureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenTwoVersionsOfProductConfigsWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledProducts.size() < 2) {
        GTEST_SKIP();
    }
    for (unsigned int product = 0; product < enabledProducts.size() - 1; product++) {
        auto config0 = enabledProducts.at(product).aotConfig;
        auto config1 = enabledProducts.at(product + 1).aotConfig;
        auto configStr0 = ProductConfigHelper::parseMajorMinorRevisionValue(config0);
        auto configStr1 = ProductConfigHelper::parseMajorMinorRevisionValue(config1);
        std::vector<ConstStringRef> expected{configStr0, configStr1};

        std::string acronymsTarget = configStr0 + "," + configStr1;
        auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(got, expected);

        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            acronymsTarget};

        testing::internal::CaptureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenProductsAcronymsWithoutDashesWhenBuildFatBinaryThenSuccessIsReturned) {
    auto acronyms = prepareProductsWithoutDashes(oclocArgHelperWithoutInput.get());
    if (acronyms.size() < 2) {
        GTEST_SKIP();
    }

    std::string acronymsTarget = acronyms[0] + "," + acronyms[1];
    std::vector<ConstStringRef> expected{ConstStringRef(acronyms[0]), ConstStringRef(acronyms[1])};

    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        acronymsTarget};

    testing::internal::CaptureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenTwoSameReleaseTargetsWhenGetProductsAcronymsThenDuplicatesAreNotFound) {
    if (enabledReleasesAcronyms.empty()) {
        GTEST_SKIP();
    }

    auto acronym = enabledReleasesAcronyms[0];
    std::vector<ConstStringRef> acronyms{};

    auto release = ProductConfigHelper::getReleaseForAcronym(acronym.str());
    getProductsAcronymsForTarget(acronyms, release, oclocArgHelperWithoutInput.get());
    auto expectedSize = acronyms.size();
    getProductsAcronymsForTarget(acronyms, release, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(acronyms.size(), expectedSize);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenTwoSameFamilyTargetsWhenGetProductsAcronymsThenDuplicatesAreNotFound) {
    if (enabledFamiliesAcronyms.empty()) {
        GTEST_SKIP();
    }

    auto acronym = enabledFamiliesAcronyms[0];
    std::vector<ConstStringRef> acronyms{};

    auto family = ProductConfigHelper::getFamilyForAcronym(acronym.str());
    getProductsAcronymsForTarget(acronyms, family, oclocArgHelperWithoutInput.get());
    auto expectedSize = acronyms.size();
    getProductsAcronymsForTarget(acronyms, family, oclocArgHelperWithoutInput.get());

    EXPECT_EQ(acronyms.size(), expectedSize);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenReleasesAcronymsWithoutDashesWhenGetProductsForFatBinaryThenCorrectAcronymsAreReturned) {
    auto acronyms = prepareReleasesWithoutDashes(oclocArgHelperWithoutInput.get());
    if (acronyms.size() < 2) {
        GTEST_SKIP();
    }

    std::vector<ConstStringRef> expected{};
    auto release0 = ProductConfigHelper::getReleaseForAcronym(acronyms[0]);
    auto release1 = ProductConfigHelper::getReleaseForAcronym(acronyms[1]);
    getProductsAcronymsForTarget(expected, release0, oclocArgHelperWithoutInput.get());
    getProductsAcronymsForTarget(expected, release1, oclocArgHelperWithoutInput.get());

    std::string releasesTarget = acronyms[0] + "," + acronyms[1];
    auto got = NEO::getTargetProductsForFatbinary(releasesTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenFamiliesAcronymsWithoutDashesWhenGetProductsForFatBinaryThenCorrectAcronymsAreReturned) {
    auto acronyms = prepareFamiliesWithoutDashes(oclocArgHelperWithoutInput.get());
    if (acronyms.size() < 2) {
        GTEST_SKIP();
    }

    std::vector<ConstStringRef> expected{};
    auto family0 = ProductConfigHelper::getFamilyForAcronym(acronyms[0]);
    auto family1 = ProductConfigHelper::getFamilyForAcronym(acronyms[1]);
    getProductsAcronymsForTarget(expected, family0, oclocArgHelperWithoutInput.get());
    getProductsAcronymsForTarget(expected, family1, oclocArgHelperWithoutInput.get());

    std::string familiesTarget = acronyms[0] + "," + acronyms[1];
    auto got = NEO::getTargetProductsForFatbinary(familiesTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenTwoTargetsOfReleasesWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledReleasesAcronyms.size() < 2) {
        GTEST_SKIP();
    }
    for (unsigned int product = 0; product < enabledReleasesAcronyms.size() - 1; product++) {
        auto acronym0 = enabledReleasesAcronyms.at(product);
        auto acronym1 = enabledReleasesAcronyms.at(product + 1);
        std::vector<ConstStringRef> expected{};

        auto release0 = ProductConfigHelper::getReleaseForAcronym(acronym0.str());
        auto release1 = ProductConfigHelper::getReleaseForAcronym(acronym1.str());
        getProductsAcronymsForTarget(expected, release0, oclocArgHelperWithoutInput.get());
        getProductsAcronymsForTarget(expected, release1, oclocArgHelperWithoutInput.get());

        std::string acronymsTarget = acronym0.str() + "," + acronym1.str();
        auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(got, expected);

        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            acronymsTarget};

        testing::internal::CaptureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenTwoTargetsOfFamiliesWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledFamiliesAcronyms.size() < 2) {
        GTEST_SKIP();
    }

    for (unsigned int product = 0; product < enabledFamiliesAcronyms.size() - 1; product++) {
        auto acronym0 = enabledFamiliesAcronyms.at(product);
        auto acronym1 = enabledFamiliesAcronyms.at(product + 1);
        std::vector<ConstStringRef> expected{};

        auto family0 = ProductConfigHelper::getFamilyForAcronym(acronym0.str());
        auto family1 = ProductConfigHelper::getFamilyForAcronym(acronym1.str());
        getProductsAcronymsForTarget(expected, family0, oclocArgHelperWithoutInput.get());
        getProductsAcronymsForTarget(expected, family1, oclocArgHelperWithoutInput.get());

        std::string acronymsTarget = acronym0.str() + "," + acronym1.str();
        auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(got, expected);

        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            acronymsTarget};

        testing::internal::CaptureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenProductsClosedRangeWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledProductsAcronyms.size() < 3) {
        GTEST_SKIP();
    }
    for (unsigned int product = 0; product < enabledProductsAcronyms.size() - 1; product++) {
        if (product == enabledProductsAcronyms.size() / 2) {
            continue;
        }
        std::vector<ConstStringRef> expected{};
        auto acronymFrom = enabledProductsAcronyms.at(product);
        auto acronymTo = enabledProductsAcronyms.at(enabledProductsAcronyms.size() / 2);

        auto prodFromIt = std::find(enabledProductsAcronyms.begin(), enabledProductsAcronyms.end(), acronymFrom);
        auto prodToIt = std::find(enabledProductsAcronyms.begin(), enabledProductsAcronyms.end(), acronymTo);
        if (prodFromIt > prodToIt) {
            std::swap(prodFromIt, prodToIt);
        }
        expected.insert(expected.end(), prodFromIt, ++prodToIt);

        std::string acronymsTarget = acronymFrom.str() + ":" + acronymTo.str();
        auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(got, expected);

        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            acronymsTarget};

        testing::internal::CaptureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenProductsClosedRangeWithoutDashesWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    auto acronyms = prepareProductsWithoutDashes(oclocArgHelperWithoutInput.get());
    if (acronyms.size() < 2) {
        GTEST_SKIP();
    }
    auto prodFromIt = std::find_if(enabledProductsAcronyms.begin(), enabledProductsAcronyms.end(), ProductConfigHelper::findAcronymWithoutDash(acronyms[0]));
    auto prodToIt = std::find_if(enabledProductsAcronyms.begin(), enabledProductsAcronyms.end(), ProductConfigHelper::findAcronymWithoutDash(acronyms[1]));
    if (prodFromIt > prodToIt) {
        std::swap(prodFromIt, prodToIt);
    }
    std::vector<ConstStringRef> expected{};
    expected.insert(expected.end(), prodFromIt, ++prodToIt);

    std::string acronymsTarget = acronyms[0] + ":" + acronyms[1];
    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        acronymsTarget};

    testing::internal::CaptureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenReleasesClosedRangeWithoutDashesWhenGetProductsForFatBinaryThenCorrectAcronymsAreReturned) {
    auto acronyms = prepareReleasesWithoutDashes(oclocArgHelperWithoutInput.get());
    if (acronyms.size() < 2) {
        GTEST_SKIP();
    }
    auto releaseFromIt = ProductConfigHelper::getReleaseForAcronym(acronyms[0]);
    auto releaseToIt = ProductConfigHelper::getReleaseForAcronym(acronyms[1]);

    if (releaseFromIt > releaseToIt) {
        std::swap(releaseFromIt, releaseToIt);
    }
    std::vector<ConstStringRef> expected{};
    while (releaseFromIt <= releaseToIt) {
        getProductsAcronymsForTarget(expected, releaseFromIt, oclocArgHelperWithoutInput.get());
        releaseFromIt = static_cast<AOT::RELEASE>(static_cast<unsigned int>(releaseFromIt) + 1);
    }

    std::string acronymsTarget = acronyms[0] + ":" + acronyms[1];
    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenFamiliesClosedRangeWithoutDashesWhenGetProductsForFatBinaryThenCorrectAcronymsAreReturned) {
    auto acronyms = prepareFamiliesWithoutDashes(oclocArgHelperWithoutInput.get());
    if (acronyms.size() < 2) {
        GTEST_SKIP();
    }

    auto familyFromIt = ProductConfigHelper::getFamilyForAcronym(acronyms[0]);
    auto familyToIt = ProductConfigHelper::getFamilyForAcronym(acronyms[1]);

    if (familyFromIt > familyToIt) {
        std::swap(familyFromIt, familyToIt);
    }
    std::vector<ConstStringRef> expected{};
    while (familyFromIt <= familyToIt) {
        getProductsAcronymsForTarget(expected, familyFromIt, oclocArgHelperWithoutInput.get());
        familyFromIt = static_cast<AOT::FAMILY>(static_cast<unsigned int>(familyFromIt) + 1);
    }

    std::string acronymsTarget = acronyms[0] + ":" + acronyms[1];
    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenFamiliesClosedRangeWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledFamiliesAcronyms.size() < 3) {
        GTEST_SKIP();
    }
    for (unsigned int family = 0; family < enabledFamiliesAcronyms.size() - 1; family++) {
        if (family == enabledFamiliesAcronyms.size() / 2) {
            continue;
        }
        std::vector<ConstStringRef> expected{};
        auto acronymFrom = enabledFamiliesAcronyms.at(family);
        auto acronymTo = enabledFamiliesAcronyms.at(enabledFamiliesAcronyms.size() / 2);

        auto familyFromIt = ProductConfigHelper::getFamilyForAcronym(acronymFrom.str());
        auto familyToIt = ProductConfigHelper::getFamilyForAcronym(acronymTo.str());

        if (familyFromIt > familyToIt) {
            std::swap(familyFromIt, familyToIt);
        }
        while (familyFromIt <= familyToIt) {
            getProductsAcronymsForTarget(expected, familyFromIt, oclocArgHelperWithoutInput.get());
            familyFromIt = static_cast<AOT::FAMILY>(static_cast<unsigned int>(familyFromIt) + 1);
        }

        std::string acronymsTarget = acronymFrom.str() + ":" + acronymTo.str();
        auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(got, expected);

        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            acronymsTarget};

        testing::internal::CaptureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeFromProductWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledProductsAcronyms.size() < 2) {
        GTEST_SKIP();
    }
    for (auto acronymIt = enabledProductsAcronyms.begin(); acronymIt != enabledProductsAcronyms.end(); ++acronymIt) {
        std::vector<ConstStringRef> expected{};
        expected.insert(expected.end(), acronymIt, enabledProductsAcronyms.end());

        std::string acronymsTarget = (*acronymIt).str() + ":";
        auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(got, expected);

        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            acronymsTarget};

        testing::internal::CaptureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeFromProductWithoutDashesWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    auto acronyms = prepareProductsWithoutDashes(oclocArgHelperWithoutInput.get());
    if (acronyms.empty()) {
        GTEST_SKIP();
    }
    std::string acronymsTarget = acronyms[0] + ":";
    std::vector<ConstStringRef> expected{};

    auto acronymIt = std::find_if(enabledProductsAcronyms.begin(), enabledProductsAcronyms.end(), ProductConfigHelper::findAcronymWithoutDash(acronyms[0]));
    expected.insert(expected.end(), acronymIt, enabledProductsAcronyms.end());

    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        acronymsTarget};

    testing::internal::CaptureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeToProductWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledProductsAcronyms.size() < 2) {
        GTEST_SKIP();
    }
    for (auto acronymIt = enabledProductsAcronyms.begin(); acronymIt != enabledProductsAcronyms.end(); ++acronymIt) {
        std::vector<ConstStringRef> expected{};
        expected.insert(expected.end(), enabledProductsAcronyms.begin(), acronymIt + 1);

        std::string acronymsTarget = ":" + (*acronymIt).str();
        auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(got, expected);

        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            acronymsTarget};

        testing::internal::CaptureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeFromReleaseWithoutDashesWhenGetProductsForFatBinaryThenCorrectAcronymsAreReturned) {
    auto acronyms = prepareReleasesWithoutDashes(oclocArgHelperWithoutInput.get());
    if (acronyms.empty()) {
        GTEST_SKIP();
    }
    std::vector<ConstStringRef> expected{};

    auto releaseFromId = ProductConfigHelper::getReleaseForAcronym(acronyms[0]);
    auto releaseToId = AOT::RELEASE_MAX;
    while (releaseFromId < releaseToId) {
        getProductsAcronymsForTarget(expected, releaseFromId, oclocArgHelperWithoutInput.get());
        releaseFromId = static_cast<AOT::RELEASE>(static_cast<unsigned int>(releaseFromId) + 1);
    }

    std::string releasesTarget = acronyms[0] + ":";
    auto got = NEO::getTargetProductsForFatbinary(releasesTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeFromReleaseWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledReleasesAcronyms.size() < 3) {
        GTEST_SKIP();
    }

    for (const auto &release : enabledReleasesAcronyms) {
        std::vector<ConstStringRef> expected{};

        auto releaseFromId = ProductConfigHelper::getReleaseForAcronym(release.str());
        auto releaseToId = AOT::RELEASE_MAX;
        while (releaseFromId < releaseToId) {
            getProductsAcronymsForTarget(expected, releaseFromId, oclocArgHelperWithoutInput.get());
            releaseFromId = static_cast<AOT::RELEASE>(static_cast<unsigned int>(releaseFromId) + 1);
        }

        std::string releasesTarget = release.str() + ":";
        auto got = NEO::getTargetProductsForFatbinary(releasesTarget, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(got, expected);

        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            releasesTarget};

        testing::internal::CaptureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeToReleaseWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledReleasesAcronyms.size() < 3) {
        GTEST_SKIP();
    }

    for (const auto &release : enabledReleasesAcronyms) {
        std::vector<ConstStringRef> expected{};

        auto releaseFromId = static_cast<AOT::RELEASE>(static_cast<unsigned int>(AOT::UNKNOWN_RELEASE) + 1);
        auto releaseToId = ProductConfigHelper::getReleaseForAcronym(release.str());

        while (releaseFromId <= releaseToId) {
            getProductsAcronymsForTarget(expected, releaseFromId, oclocArgHelperWithoutInput.get());
            releaseFromId = static_cast<AOT::RELEASE>(static_cast<unsigned int>(releaseFromId) + 1);
        }

        std::string releasesTarget = ":" + release.str();
        auto got = NEO::getTargetProductsForFatbinary(releasesTarget, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(got, expected);

        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            releasesTarget};

        testing::internal::CaptureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeFromFamilyWithoutDashesWhenGetProductsForFatBinaryThenCorrectAcronymsAreReturned) {
    auto acronyms = prepareFamiliesWithoutDashes(oclocArgHelperWithoutInput.get());
    if (acronyms.empty()) {
        GTEST_SKIP();
    }
    std::vector<ConstStringRef> expected{};

    auto familyFromId = ProductConfigHelper::getFamilyForAcronym(acronyms[0]);
    auto familyToId = AOT::FAMILY_MAX;
    while (familyFromId < familyToId) {
        getProductsAcronymsForTarget(expected, familyFromId, oclocArgHelperWithoutInput.get());
        familyFromId = static_cast<AOT::FAMILY>(static_cast<unsigned int>(familyFromId) + 1);
    }

    std::string familiesTarget = acronyms[0] + ":";
    auto got = NEO::getTargetProductsForFatbinary(familiesTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeFromFamilyWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledFamiliesAcronyms.size() < 3) {
        GTEST_SKIP();
    }

    for (const auto &family : enabledFamiliesAcronyms) {
        std::vector<ConstStringRef> expected{};

        auto familyFromId = ProductConfigHelper::getFamilyForAcronym(family.str());
        auto familyToId = AOT::FAMILY_MAX;
        while (familyFromId < familyToId) {
            getProductsAcronymsForTarget(expected, familyFromId, oclocArgHelperWithoutInput.get());
            familyFromId = static_cast<AOT::FAMILY>(static_cast<unsigned int>(familyFromId) + 1);
        }

        std::string familiesTarget = family.str() + ":";
        auto got = NEO::getTargetProductsForFatbinary(familiesTarget, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(got, expected);

        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            familiesTarget};

        testing::internal::CaptureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeToFamilyWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledFamiliesAcronyms.size() < 3) {
        GTEST_SKIP();
    }

    for (const auto &family : enabledFamiliesAcronyms) {
        std::vector<ConstStringRef> expected{};

        auto familyFromId = static_cast<AOT::FAMILY>(static_cast<unsigned int>(AOT::UNKNOWN_FAMILY) + 1);
        auto familyToId = ProductConfigHelper::getFamilyForAcronym(family.str());

        while (familyFromId <= familyToId) {
            getProductsAcronymsForTarget(expected, familyFromId, oclocArgHelperWithoutInput.get());
            familyFromId = static_cast<AOT::FAMILY>(static_cast<unsigned int>(familyFromId) + 1);
        }

        std::string familiesTarget = ":" + family.str();
        auto got = NEO::getTargetProductsForFatbinary(familiesTarget, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(got, expected);

        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            familiesTarget};

        testing::internal::CaptureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryTest, givenSpirvInputWhenFatBinaryIsRequestedThenArchiveContainsGenericIrFileWithSpirvContent) {
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

    mockArgHelper.getPrinterRef() = MessagePrinter{true};
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

TEST_F(OclocFatBinaryTest, givenDeviceFlagWithoutConsecutiveArgumentWhenBuildingFatbinaryThenErrorIsReported) {
    const std::vector<std::string> args = {
        "ocloc",
        "-device"};

    ::testing::internal::CaptureStdout();
    const auto result = buildFatBinary(args, &mockArgHelper);
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, result);

    const std::string expectedErrorMessage{"Error! Command does not contain device argument!\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OclocFatBinaryTest, givenFlagsWhichRequireMoreArgsWithoutThemWhenBuildingFatbinaryThenErrorIsReported) {
    const auto devices = prepareTwoDevices(&mockArgHelper);
    if (devices.empty()) {
        GTEST_SKIP();
    }
    const std::array<std::string, 3> flagsToTest = {"-file", "-output", "-out_dir"};

    for (const auto &flag : flagsToTest) {
        const std::vector<std::string> args = {
            "ocloc",
            "-device",
            devices,
            flag};

        ::testing::internal::CaptureStdout();
        const auto result = buildFatBinary(args, &mockArgHelper);
        const auto output{::testing::internal::GetCapturedStdout()};

        EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, result);

        const std::string expectedErrorMessage{"Invalid option (arg 3): " + flag + "\nError! Couldn't create OfflineCompiler. Exiting.\n"};
        EXPECT_EQ(expectedErrorMessage, output);
    }
}

TEST_F(OclocFatBinaryTest, givenBitFlagsWhenBuildingFatbinaryThenFilesInArchiveHaveCorrectPointerSize) {
    const auto devices = prepareTwoDevices(&mockArgHelper);
    if (devices.empty()) {
        GTEST_SKIP();
    }
    using TestDescription = std::pair<std::string, std::string>;

    const std::array<TestDescription, 4> flagsToTest{
        TestDescription{"-32", "32"},
        TestDescription{NEO::CompilerOptions::arch32bit.str(), "32"},
        TestDescription{"-64", "64"},
        TestDescription{NEO::CompilerOptions::arch64bit.str(), "64"}};

    for (const auto &[flag, expectedFilePrefix] : flagsToTest) {
        const std::vector<std::string> args = {
            "ocloc",
            "-output",
            outputArchiveName,
            "-file",
            spirvFilename,
            "-output_no_suffix",
            "-spirv_input",
            "-exclude_ir",
            flag,
            "-device",
            devices};

        mockArgHelper.getPrinterRef() = MessagePrinter{true};
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

        EXPECT_TRUE(allFilesInArchiveExceptPaddingStartsWith(decodedArchive, expectedFilePrefix));
    }
}

TEST_F(OclocFatBinaryTest, givenOutputDirectoryFlagWhenBuildingFatbinaryThenArchiveIsStoredInThatDirectory) {
    const auto devices = prepareTwoDevices(&mockArgHelper);
    if (devices.empty()) {
        GTEST_SKIP();
    }
    const std::string outputDirectory{"someOutputDir"};

    const std::vector<std::string> args = {
        "ocloc",
        "-output",
        outputArchiveName,
        "-out_dir",
        outputDirectory,
        "-file",
        spirvFilename,
        "-output_no_suffix",
        "-spirv_input",
        "-device",
        devices};

    mockArgHelper.getPrinterRef() = MessagePrinter{true};
    const auto buildResult = buildFatBinary(args, &mockArgHelper);
    ASSERT_EQ(OclocErrorCode::SUCCESS, buildResult);

    const auto expectedArchivePath{outputDirectory + "/" + outputArchiveName};
    ASSERT_EQ(1u, mockArgHelper.interceptedFiles.count(expectedArchivePath));
}

TEST_F(OclocFatBinaryTest, givenSpirvInputAndExcludeIrFlagWhenFatBinaryIsRequestedThenArchiveDoesNotContainGenericIrFile) {
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

    mockArgHelper.getPrinterRef() = MessagePrinter{true};
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

TEST_F(OclocFatBinaryTest, givenClInputFileWhenFatBinaryIsRequestedThenArchiveDoesNotContainGenericIrFile) {
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

    mockArgHelper.getPrinterRef() = MessagePrinter{true};
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

TEST_F(OclocFatBinaryTest, givenEmptyFileWhenAppendingGenericIrThenInvalidFileIsReturned) {
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

TEST_F(OclocFatBinaryTest, givenInvalidIrFileWhenAppendingGenericIrThenInvalidFileIsReturned) {
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

TEST(OclocFatBinaryHelpersTest, givenPreviousCompilationErrorWhenBuildingFatbinaryForTargetThenNothingIsDoneAndErrorIsReturned) {
    const std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.argHelper->getPrinterRef() = MessagePrinter{true};
    mockOfflineCompiler.initialize(argv.size(), argv);

    // We expect that nothing is done and error is returned.
    // Therefore, if offline compiler is used, ensure that it just returns error code,
    // which is different than expected one.
    mockOfflineCompiler.buildReturnValue = OclocErrorCode::SUCCESS;

    Ar::ArEncoder ar;
    const std::string pointerSize{"32"};
    const auto mockArgHelper = mockOfflineCompiler.uniqueHelper.get();
    const auto deviceConfig = getDeviceConfig(mockOfflineCompiler, mockArgHelper);

    const int previousReturnValue{OclocErrorCode::INVALID_FILE};
    const auto buildResult = buildFatBinaryForTarget(previousReturnValue, argv, pointerSize, ar, &mockOfflineCompiler, mockArgHelper, deviceConfig);

    EXPECT_EQ(OclocErrorCode::INVALID_FILE, buildResult);
    EXPECT_EQ(0, mockOfflineCompiler.buildCalledCount);
}

TEST(OclocFatBinaryHelpersTest, givenPreviousCompilationSuccessAndFailingBuildWhenBuildingFatbinaryForTargetThenCompilationIsInvokedAndErrorLogIsPrinted) {
    const std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    mockOfflineCompiler.buildReturnValue = OclocErrorCode::INVALID_FILE;

    Ar::ArEncoder ar;
    const std::string pointerSize{"32"};
    const auto mockArgHelper = mockOfflineCompiler.uniqueHelper.get();
    const auto deviceConfig = getDeviceConfig(mockOfflineCompiler, mockArgHelper);

    ::testing::internal::CaptureStdout();
    const int previousReturnValue{OclocErrorCode::SUCCESS};
    const auto buildResult = buildFatBinaryForTarget(previousReturnValue, argv, pointerSize, ar, &mockOfflineCompiler, mockArgHelper, deviceConfig);
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::INVALID_FILE, buildResult);
    EXPECT_EQ(1, mockOfflineCompiler.buildCalledCount);

    std::string commandString{};
    for (const auto &arg : argv) {
        commandString += " ";
        commandString += arg;
    }

    const std::string expectedOutput{
        "Build failed for : " + deviceConfig + " with error code: -5151\n"
                                               "Command was:" +
        commandString + "\n"};
    EXPECT_EQ(expectedOutput, output);
}

TEST(OclocFatBinaryHelpersTest, givenNonEmptyBuildLogWhenBuildingFatbinaryForTargetThenBuildLogIsPrinted) {
    using namespace std::string_literals;

    const std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    const char buildWarning[] = "Warning: This is a build log!";
    mockOfflineCompiler.updateBuildLog(buildWarning, sizeof(buildWarning));
    mockOfflineCompiler.buildReturnValue = OclocErrorCode::SUCCESS;

    // Dummy value
    mockOfflineCompiler.elfBinary = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    Ar::ArEncoder ar;
    const std::string pointerSize{"32"};
    const auto mockArgHelper = mockOfflineCompiler.uniqueHelper.get();
    const auto deviceConfig = getDeviceConfig(mockOfflineCompiler, mockArgHelper);

    ::testing::internal::CaptureStdout();
    const int previousReturnValue{OclocErrorCode::SUCCESS};
    const auto buildResult = buildFatBinaryForTarget(previousReturnValue, argv, pointerSize, ar, &mockOfflineCompiler, mockArgHelper, deviceConfig);
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::SUCCESS, buildResult);
    EXPECT_EQ(1, mockOfflineCompiler.buildCalledCount);

    const std::string expectedOutput{buildWarning + "\nBuild succeeded for : "s + deviceConfig + ".\n"s};
    EXPECT_EQ(expectedOutput, output);
}

TEST(OclocFatBinaryHelpersTest, givenQuietModeWhenBuildingFatbinaryForTargetThenNothingIsPrinted) {
    using namespace std::string_literals;

    const std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-q",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    // Dummy value
    mockOfflineCompiler.elfBinary = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    mockOfflineCompiler.buildReturnValue = OclocErrorCode::SUCCESS;

    Ar::ArEncoder ar;
    const std::string pointerSize{"32"};
    const auto mockArgHelper = mockOfflineCompiler.uniqueHelper.get();
    const auto deviceConfig = getDeviceConfig(mockOfflineCompiler, mockArgHelper);

    ::testing::internal::CaptureStdout();
    const int previousReturnValue{OclocErrorCode::SUCCESS};
    const auto buildResult = buildFatBinaryForTarget(previousReturnValue, argv, pointerSize, ar, &mockOfflineCompiler, mockArgHelper, deviceConfig);
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::SUCCESS, buildResult);
    EXPECT_EQ(1, mockOfflineCompiler.buildCalledCount);

    EXPECT_TRUE(output.empty()) << output;
}

} // namespace NEO