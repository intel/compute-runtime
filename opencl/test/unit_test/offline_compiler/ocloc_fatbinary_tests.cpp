/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/ocloc_fatbinary_tests.h"

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "environment.h"
#include "mock/mock_argument_helper.h"
#include "mock/mock_offline_compiler.h"
#include "neo_aot_platforms.h"

#include <algorithm>
#include <filesystem>
#include <unordered_set>

extern Environment *gEnvironment;

namespace NEO {
extern std::map<std::string, std::stringstream> virtualFileList;

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

template <typename EqComparableT>
static auto findAcronymForEnum(const EqComparableT &lhs) {
    return [&lhs](const auto &rhs) { return lhs == rhs.second; };
}

TEST(OclocFatBinaryIsSpvOnly, givenSpvOnlyProvidedReturnsTrue) {
    std::vector<std::string> args = {"-spv_only"};

    EXPECT_TRUE(NEO::isSpvOnly(args));
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
    std::unique_ptr<CompilerProductHelper> compilerProductHelper;
    std::unique_ptr<ReleaseHelper> releaseHelper;

    NEO::HardwareInfo hwInfo;

    EXPECT_FALSE(argHelper->setHwInfoForProductConfig(AOT::UNKNOWN_ISA, hwInfo, compilerProductHelper, releaseHelper));
}

TEST(OclocFatBinaryRequestedFatBinary, givenReleaseOrFamilyAcronymWhenGetAcronymsForTargetThenCorrectValuesAreReturned) {
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto &enabledDeviceConfigs = argHelper->productConfigHelper->getDeviceAotInfo();
    if (enabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    std::vector<ConstStringRef> outRelease{}, outFamily{};

    for (auto &device : enabledDeviceConfigs) {
        ConstStringRef acronym("");
        auto hasDeviceAcronym = std::any_of(enabledDeviceConfigs.begin(), enabledDeviceConfigs.end(), ProductConfigHelper::findDeviceAcronymForRelease(device.release));

        if (!device.deviceAcronyms.empty()) {
            acronym = device.deviceAcronyms.front();
        } else if (!device.rtlIdAcronyms.empty() && !hasDeviceAcronym) {
            acronym = device.rtlIdAcronyms.front();
        }

        if (!acronym.empty()) {
            getProductsAcronymsForTarget<AOT::RELEASE>(outRelease, device.release, argHelper.get());
            EXPECT_TRUE(std::find(outRelease.begin(), outRelease.end(), acronym) != outRelease.end()) << acronym.str();

            getProductsAcronymsForTarget<AOT::FAMILY>(outFamily, device.family, argHelper.get());
            EXPECT_TRUE(std::find(outFamily.begin(), outFamily.end(), acronym) != outFamily.end()) << acronym.str();

            device.deviceAcronyms.clear();
            device.rtlIdAcronyms.clear();
            outRelease.clear();
            outFamily.clear();

            getProductsAcronymsForTarget<AOT::RELEASE>(outRelease, device.release, argHelper.get());
            EXPECT_FALSE(std::find(outRelease.begin(), outRelease.end(), acronym) != outRelease.end()) << acronym.str();

            getProductsAcronymsForTarget<AOT::FAMILY>(outFamily, device.family, argHelper.get());
            EXPECT_FALSE(std::find(outFamily.begin(), outFamily.end(), acronym) != outFamily.end()) << acronym.str();
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
    majorString << aotConfig.architecture;
    auto major = majorString.str();

    auto aotValue0 = argHelper->productConfigHelper->getProductConfigFromVersionValue(major);
    EXPECT_EQ(aotValue0, AOT::UNKNOWN_ISA);

    auto majorMinor = ProductConfigHelper::parseMajorMinorValue(aotConfig);
    auto aotValue1 = argHelper->productConfigHelper->getProductConfigFromVersionValue(majorMinor);
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

        for (const auto &acronym : deviceConfig.deviceAcronyms) {
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

        oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
        std::stringstream acronymsString;
        acronymsString << enabledAcronyms[0].str() << ":" << enabledAcronyms[1].str() << ":" << enabledAcronyms[2].str();
        auto target = acronymsString.str();

        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clCopybufferFilename.c_str(),
            "-device",
            target};

        StreamCapture capture;
        capture.captureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = capture.getCapturedStdout();
        EXPECT_NE(retVal, OCLOC_SUCCESS);
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

    std::vector<ConstStringRef> expected{};
    expected.insert(expected.end(), enabledProductsAcronyms.begin(), enabledProductsAcronyms.begin() + 2);

    std::string acronym0 = enabledProductsAcronyms.front().str();
    std::string acronym1 = (enabledProductsAcronyms.begin() + 1)->str();

    std::string acronymsTarget = acronym0 + "," + acronym1;
    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        acronymsTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenTwoVersionsOfProductConfigsWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledProducts.size() < 2) {
        GTEST_SKIP();
    }

    auto config0 = enabledProducts.at(0).aotConfig;
    auto config1 = enabledProducts.at(1).aotConfig;

    auto configStr0 = ProductConfigHelper::parseMajorMinorRevisionValue(config0);
    auto configStr1 = ProductConfigHelper::parseMajorMinorRevisionValue(config1);

    std::vector<ConstStringRef> expected{configStr0, configStr1};

    std::string acronymsTarget = configStr0 + "," + configStr1;
    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        acronymsTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
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

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        acronymsTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenBinaryOutputNameOptionWhenBuildingThenCorrectFileIsCreated) {
    if (enabledProductsAcronyms.size() < 2) {
        GTEST_SKIP();
    }

    std::vector<ConstStringRef> expected{};
    expected.insert(expected.end(), enabledProductsAcronyms.begin(), enabledProductsAcronyms.begin() + 2);

    std::string acronym0 = enabledProductsAcronyms.front().str();
    std::string acronym1 = (enabledProductsAcronyms.begin() + 1)->str();

    std::string acronymsTarget = acronym0 + "," + acronym1;
    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-o",
        "expected_output.bin",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        acronymsTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    EXPECT_EQ(4u, NEO::virtualFileList.size());
    EXPECT_TRUE(NEO::virtualFileList.find("expected_output.bin") != NEO::virtualFileList.end());

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenBinaryOutputDirOptionWhenBuildingThenCorrectFileIsCreated) {
    auto acronyms = prepareProductsWithoutDashes(oclocArgHelperWithoutInput.get());
    if (acronyms.size() < 2) {
        GTEST_SKIP();
    }

    std::string acronymsTarget = acronyms[0] + "," + acronyms[1];
    std::vector<ConstStringRef> expected{ConstStringRef(acronyms[0]), ConstStringRef(acronyms[1])};
    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);

    {
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clCopybufferFilename.c_str(),
            "-out_dir",
            "../expected_output_directory",
            "-device",
            acronymsTarget};

        StreamCapture capture;
        capture.captureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = capture.getCapturedStdout();
        EXPECT_EQ(retVal, OCLOC_SUCCESS);

        const std::string expectedFatbinaryFileName = "../expected_output_directory/some_kernel.ar";
        EXPECT_EQ(4u, NEO::virtualFileList.size());
        EXPECT_TRUE(NEO::virtualFileList.find(expectedFatbinaryFileName) != NEO::virtualFileList.end());

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }

    {
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clCopybufferFilename.c_str(),
            "-out_dir",
            "../expected_output_directory",
            "-output",
            "expected_filename",
            "-device",
            acronymsTarget};

        StreamCapture capture;
        capture.captureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = capture.getCapturedStdout();
        EXPECT_EQ(retVal, OCLOC_SUCCESS);

        const std::string expectedFatbinaryFileName = "../expected_output_directory/expected_filename";
        EXPECT_EQ(5u, NEO::virtualFileList.size());
        EXPECT_TRUE(NEO::virtualFileList.find(expectedFatbinaryFileName) != NEO::virtualFileList.end());

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenTwoSameReleaseTargetsWhenGetProductsAcronymsThenDuplicatesAreNotFound) {
    if (enabledReleasesAcronyms.empty()) {
        GTEST_SKIP();
    }

    auto acronym = enabledReleasesAcronyms[0];
    std::vector<ConstStringRef> acronyms{};

    auto release = oclocArgHelperWithoutInput->productConfigHelper->getReleaseFromDeviceName(acronym.str());
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

    auto family = oclocArgHelperWithoutInput->productConfigHelper->getFamilyFromDeviceName(acronym.str());
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
    auto release0 = oclocArgHelperWithoutInput->productConfigHelper->getReleaseFromDeviceName(acronyms[0]);
    auto release1 = oclocArgHelperWithoutInput->productConfigHelper->getReleaseFromDeviceName(acronyms[1]);
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
    auto family0 = oclocArgHelperWithoutInput->productConfigHelper->getFamilyFromDeviceName(acronyms[0]);
    auto family1 = oclocArgHelperWithoutInput->productConfigHelper->getFamilyFromDeviceName(acronyms[1]);
    getProductsAcronymsForTarget(expected, family0, oclocArgHelperWithoutInput.get());
    getProductsAcronymsForTarget(expected, family1, oclocArgHelperWithoutInput.get());

    std::string familiesTarget = acronyms[0] + "," + acronyms[1];
    auto got = NEO::getTargetProductsForFatbinary(familiesTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenTwoTargetsOfReleasesWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });
    if (enabledReleasesAcronyms.size() < 2) {
        GTEST_SKIP();
    }
    std::vector<ConstStringRef> expected{};

    std::string acronym0 = enabledReleasesAcronyms.front().str();
    std::string acronym1 = (enabledReleasesAcronyms.begin() + 1)->str();

    auto release0 = oclocArgHelperWithoutInput->productConfigHelper->getReleaseFromDeviceName(acronym0);
    auto release1 = oclocArgHelperWithoutInput->productConfigHelper->getReleaseFromDeviceName(acronym1);
    getProductsAcronymsForTarget(expected, release0, oclocArgHelperWithoutInput.get());
    getProductsAcronymsForTarget(expected, release1, oclocArgHelperWithoutInput.get());

    std::string acronymsTarget = acronym0 + "," + acronym1;
    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        acronymsTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenTwoTargetsOfFamiliesWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });
    if (enabledFamiliesAcronyms.size() < 2) {
        GTEST_SKIP();
    }

    std::string acronym0 = enabledFamiliesAcronyms.front().str();
    std::string acronym1 = (enabledFamiliesAcronyms.begin() + 1)->str();

    std::vector<ConstStringRef> expected{};

    auto family0 = oclocArgHelperWithoutInput->productConfigHelper->getFamilyFromDeviceName(acronym0);
    auto family1 = oclocArgHelperWithoutInput->productConfigHelper->getFamilyFromDeviceName(acronym1);
    getProductsAcronymsForTarget(expected, family0, oclocArgHelperWithoutInput.get());
    getProductsAcronymsForTarget(expected, family1, oclocArgHelperWithoutInput.get());

    std::string acronymsTarget = acronym0 + "," + acronym1;
    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        acronymsTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenProductsClosedRangeWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledProductsAcronyms.size() < 3) {
        GTEST_SKIP();
    }

    auto acronymFrom = enabledProductsAcronyms.at(0);
    auto acronymTo = enabledProductsAcronyms.at(2);

    auto prodFromIt = std::find(enabledProductsAcronyms.begin(), enabledProductsAcronyms.end(), acronymFrom);
    auto prodToIt = std::find(enabledProductsAcronyms.begin(), enabledProductsAcronyms.end(), acronymTo);
    if (prodFromIt > prodToIt) {
        std::swap(prodFromIt, prodToIt);
    }

    std::vector<ConstStringRef> expected{};
    expected.insert(expected.end(), prodFromIt, ++prodToIt);

    std::string acronymsTarget = acronymFrom.str() + ":" + acronymTo.str();
    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        acronymsTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
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

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        acronymsTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

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
    auto releaseFromIt = oclocArgHelperWithoutInput->productConfigHelper->getReleaseFromDeviceName(acronyms[0]);
    auto releaseToIt = oclocArgHelperWithoutInput->productConfigHelper->getReleaseFromDeviceName(acronyms[1]);

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

    auto familyFromIt = oclocArgHelperWithoutInput->productConfigHelper->getFamilyFromDeviceName(acronyms[0]);
    auto familyToIt = oclocArgHelperWithoutInput->productConfigHelper->getFamilyFromDeviceName(acronyms[1]);

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

TEST_F(OclocFatBinaryProductAcronymsTests, givenClosedRangeWithOneFamilyBeingGen12lpLegacyAliasWhenGetProductsForFatBinaryThenCorrectAcronymsAreReturned) {
    if (enabledFamiliesAcronyms.size() < 2) {
        GTEST_SKIP();
    }

    auto familiesToReleases = {
        std::make_tuple("gen12lp:xe", AOT::XE_FAMILY, AOT::XE_LP_RELEASE, AOT::XE_LPGPLUS_RELEASE),
        std::make_tuple("xe:gen12lp", AOT::XE_FAMILY, AOT::XE_LP_RELEASE, AOT::XE_LPGPLUS_RELEASE),
    };

    for (auto [acronymsTarget, requiredFamily, releaseFrom, releaseTo] : familiesToReleases) {
        if (std::find(enabledFamiliesAcronyms.begin(), enabledFamiliesAcronyms.end(), requiredFamily) == enabledFamiliesAcronyms.end()) {
            continue;
        }

        std::vector<ConstStringRef> expected{};
        while (releaseFrom <= releaseTo) {
            getProductsAcronymsForTarget(expected, releaseFrom, oclocArgHelperWithoutInput.get());
            releaseFrom = static_cast<AOT::RELEASE>(static_cast<unsigned int>(releaseFrom) + 1);
        }
        auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(got, expected);

        oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
        std::stringstream resString;
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clCopybufferFilename.c_str(),
            "-device",
            acronymsTarget};

        StreamCapture capture;
        capture.captureStdout();
        int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
        auto output = capture.getCapturedStdout();
        EXPECT_EQ(retVal, OCLOC_SUCCESS);

        for (const auto &product : expected) {
            resString << "Build succeeded for : " << product.str() + ".\n";
        }

        EXPECT_STREQ(output.c_str(), resString.str().c_str());
    }
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenFamiliesClosedRangeWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledFamiliesAcronyms.size() < 3) {
        GTEST_SKIP();
    }

    std::vector<ConstStringRef> expected{};
    auto acronymFrom = enabledFamiliesAcronyms.at(0);
    auto acronymTo = enabledFamiliesAcronyms.at(2);

    auto familyFromIt = oclocArgHelperWithoutInput->productConfigHelper->getFamilyFromDeviceName(acronymFrom.str());
    auto familyToIt = oclocArgHelperWithoutInput->productConfigHelper->getFamilyFromDeviceName(acronymTo.str());

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

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        acronymsTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeFromProductWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledProductsAcronyms.size() < 2) {
        GTEST_SKIP();
    }

    std::vector<ConstStringRef> expected{};
    expected.insert(expected.end(), enabledProductsAcronyms.end() - 2, enabledProductsAcronyms.end());

    std::string acronymsTarget = (enabledProductsAcronyms.end() - 2)->str() + ":";
    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        acronymsTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str()) << ConstStringRef(" ").join(argv);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeFromProductWithoutDashesWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });
    auto acronyms = prepareProductsWithoutDashes(oclocArgHelperWithoutInput.get());
    if (acronyms.size() < 2) {
        GTEST_SKIP();
    }
    std::string acronym = acronyms[acronyms.size() - 2];
    std::string acronymsTarget = acronym + ":";
    std::vector<ConstStringRef> expected{};

    auto acronymIt = std::find_if(enabledProductsAcronyms.begin(), enabledProductsAcronyms.end(), ProductConfigHelper::findAcronymWithoutDash(acronym));
    expected.insert(expected.end(), acronymIt, enabledProductsAcronyms.end());

    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        acronymsTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeToProductWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledProductsAcronyms.size() < 2) {
        GTEST_SKIP();
    }
    auto acronymTo = enabledProductsAcronyms.at(1);

    std::vector<ConstStringRef> expected{};
    expected.insert(expected.end(), enabledProductsAcronyms.begin(), enabledProductsAcronyms.begin() + 2);

    std::string acronymsTarget = ":" + acronymTo.str();
    auto got = NEO::getTargetProductsForFatbinary(acronymsTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        acronymsTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeFromReleaseWithoutDashesWhenGetProductsForFatBinaryThenCorrectAcronymsAreReturned) {
    auto acronyms = prepareReleasesWithoutDashes(oclocArgHelperWithoutInput.get());
    if (acronyms.empty()) {
        GTEST_SKIP();
    }
    std::vector<ConstStringRef> expected{};

    auto releaseFromId = oclocArgHelperWithoutInput->productConfigHelper->getReleaseFromDeviceName(acronyms[0]);
    auto releaseToId = AOT::RELEASE_MAX;
    while (releaseFromId < releaseToId) {
        getProductsAcronymsForTarget(expected, releaseFromId, oclocArgHelperWithoutInput.get());
        releaseFromId = static_cast<AOT::RELEASE>(static_cast<unsigned int>(releaseFromId) + 1);
    }

    std::string releasesTarget = acronyms[0] + ":";
    auto got = NEO::getTargetProductsForFatbinary(releasesTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenReleaseValueWhenGetProductsAcronymsAndNoDeviceAcronymsAreFoundThenCorrectResultIsReturnedWithNoDuplicates) {
    auto &aotInfos = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    if (aotInfos.empty()) {
        GTEST_SKIP();
    }
    auto release = aotInfos[0].release;

    for (auto &aotInfo : aotInfos) {
        if (aotInfo.release == release) {
            aotInfo.deviceAcronyms.clear();
            aotInfo.rtlIdAcronyms.clear();
        }
    }

    std::vector<ConstStringRef> acronyms{};
    getProductsAcronymsForTarget(acronyms, release, oclocArgHelperWithoutInput.get());

    EXPECT_TRUE(acronyms.empty());

    std::string tmpStr("tmp");
    aotInfos[0].rtlIdAcronyms.push_back(ConstStringRef(tmpStr));

    getProductsAcronymsForTarget(acronyms, release, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(acronyms.size(), 1u);
    EXPECT_EQ(acronyms.front(), tmpStr);

    getProductsAcronymsForTarget(acronyms, release, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(acronyms.size(), 1u);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenFullRangeWhenGetProductsForRangeThenCorrectResultIsReturned) {
    auto &aotInfos = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    if (aotInfos.empty()) {
        GTEST_SKIP();
    }
    auto product = aotInfos[0].aotConfig.value;
    uint32_t productTo = AOT::getConfixMaxPlatform();
    --productTo;
    auto got = NEO::getProductsForRange(product, static_cast<AOT::PRODUCT_CONFIG>(productTo), oclocArgHelperWithoutInput.get());

    auto expected = oclocArgHelperWithoutInput->productConfigHelper->getRepresentativeProductAcronyms();
    EXPECT_EQ(got, expected);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenProductConfigValuesWhenGetProductsForRangeAndAcronymsAreEmptyThenEmptyResultIsReturned) {
    auto &aotInfos = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    if (aotInfos.size() < 2) {
        GTEST_SKIP();
    }

    aotInfos[0].deviceAcronyms.clear();
    aotInfos[0].rtlIdAcronyms.clear();

    aotInfos[1].deviceAcronyms.clear();
    aotInfos[1].rtlIdAcronyms.clear();

    auto acronyms = NEO::getProductsForRange(aotInfos[0].aotConfig.value, aotInfos[1].aotConfig.value, oclocArgHelperWithoutInput.get());
    EXPECT_TRUE(acronyms.empty());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOnlyRtlIdAcronymsForConfigWhenGetProductsForRangeThenCorrectResultIsReturned) {
    auto &aotInfos = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    if (aotInfos.empty()) {
        GTEST_SKIP();
    }
    auto &aotInfo = aotInfos[0];
    auto product = aotInfo.aotConfig.value;

    aotInfo.deviceAcronyms.clear();
    aotInfo.rtlIdAcronyms.clear();

    std::string tmpStr("tmp");
    aotInfo.rtlIdAcronyms.push_back(ConstStringRef(tmpStr));

    uint32_t productTo = AOT::getConfixMaxPlatform();
    --productTo;
    auto acronyms = NEO::getProductsForRange(product, static_cast<AOT::PRODUCT_CONFIG>(productTo), oclocArgHelperWithoutInput.get());

    EXPECT_NE(std::find(acronyms.begin(), acronyms.end(), tmpStr), acronyms.end());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeFromReleaseWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledReleasesAcronyms.size() < 3) {
        GTEST_SKIP();
    }

    const auto release = enabledReleasesAcronyms[enabledReleasesAcronyms.size() - 3].str();
    std::vector<ConstStringRef> expected{};

    auto releaseFromId = oclocArgHelperWithoutInput->productConfigHelper->getReleaseFromDeviceName(release);
    auto releaseToId = AOT::RELEASE_MAX;
    while (releaseFromId < releaseToId) {
        getProductsAcronymsForTarget(expected, releaseFromId, oclocArgHelperWithoutInput.get());
        releaseFromId = static_cast<AOT::RELEASE>(static_cast<unsigned int>(releaseFromId) + 1);
    }
    if (expected.empty()) {
        GTEST_SKIP();
    }
    std::string releasesTarget = release + ":";
    auto got = NEO::getTargetProductsForFatbinary(releasesTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        releasesTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenDeviceOptionsForNotCompiledDeviceAndListOfProductsWhenFatBinaryBuildIsInvokedThenWarningIsPrinted) {
    if (enabledProductsAcronyms.size() < 3) {
        GTEST_SKIP();
    }

    std::stringstream products;
    products << enabledProductsAcronyms[0].str() << "," << enabledProductsAcronyms[1].str();

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        products.str().c_str(),
        "-device_options",
        enabledProductsAcronyms[2].str(),
        "deviceOptions"};

    StreamCapture capture;
    capture.captureStdout();
    [[maybe_unused]] int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();

    std::stringstream expectedErrorMessage;
    expectedErrorMessage << "Warning! -device_options set for non-compiled device: " + enabledProductsAcronyms[2].str() + "\n";
    EXPECT_TRUE(hasSubstr(output, expectedErrorMessage.str()));
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenDeviceOptionsForCompiledDeviceAndListOfProductsWhenFatBinaryBuildIsInvokedThenWarningIsNotPrinted) {
    if (enabledProductsAcronyms.size() < 2) {
        GTEST_SKIP();
    }

    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedtedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedtedFiles.begin(), expectedtedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedtedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

    std::stringstream products;
    products << enabledProductsAcronyms[0].str() + "," + enabledProductsAcronyms[1].str();

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        products.str().c_str(),
        "-device_options",
        enabledProductsAcronyms[0].str(),
        "deviceOptions"};

    StreamCapture capture;
    capture.captureStdout();
    [[maybe_unused]] int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();

    std::stringstream errorMessage1, errorMessage2;
    errorMessage1 << "Warning! -device_options set for non-compiled device: " << enabledProductsAcronyms[0].str() << "\n";
    errorMessage2 << "Warning! -device_options set for non-compiled device: " << enabledProductsAcronyms[1].str() << "\n";

    EXPECT_FALSE(hasSubstr(output, errorMessage1.str()));
    EXPECT_FALSE(hasSubstr(output, errorMessage2.str()));
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenDeviceOptionsForMultipleDevicesSeparatedByCommasWhenFatBinaryBuildIsInvokedThenWarningIsNotPrinted) {
    if (enabledProductsAcronyms.size() < 3) {
        GTEST_SKIP();
    }

    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedtedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedtedFiles.begin(), expectedtedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedtedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

    std::stringstream products, productsForDeviceOptions;
    products << enabledProductsAcronyms[0].str() << ","
             << enabledProductsAcronyms[1].str() << ","
             << enabledProductsAcronyms[2].str();

    productsForDeviceOptions << enabledProductsAcronyms[0].str() << ","
                             << enabledProductsAcronyms[1].str();

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        products.str().c_str(),
        "-device_options",
        productsForDeviceOptions.str().c_str(),
        "deviceOptions"};

    StreamCapture capture;
    capture.captureStdout();
    [[maybe_unused]] int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();

    std::stringstream expectedErrorMessage;
    expectedErrorMessage << "Warning! -device_options set for non-compiled device";
    EXPECT_FALSE(hasSubstr(output, expectedErrorMessage.str()));
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeToReleaseWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledReleasesAcronyms.size() < 3) {
        GTEST_SKIP();
    }

    const auto release = (enabledReleasesAcronyms.end() - 3)->str();
    std::vector<ConstStringRef> expected{};

    auto releaseFromId = static_cast<AOT::RELEASE>(static_cast<unsigned int>(AOT::UNKNOWN_RELEASE) + 1);
    auto releaseToId = oclocArgHelperWithoutInput->productConfigHelper->getReleaseFromDeviceName(release);

    while (releaseFromId <= releaseToId) {
        getProductsAcronymsForTarget(expected, releaseFromId, oclocArgHelperWithoutInput.get());
        releaseFromId = static_cast<AOT::RELEASE>(static_cast<unsigned int>(releaseFromId) + 1);
    }
    if (expected.empty()) {
        GTEST_SKIP();
    }
    std::string releasesTarget = ":" + release;
    auto got = NEO::getTargetProductsForFatbinary(releasesTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        releasesTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenReleaseWhichHasNoDeviceAcronymWhenGetTargetProductsForFatbinaryThenCorrectResultIsReturned) {
    auto &aotInfos = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    DeviceAotInfo *deviceInfo = nullptr;
    std::vector<ConstStringRef> expected{};

    for (auto &aotInfo : aotInfos) {
        auto hasDeviceAcronym = std::any_of(aotInfos.begin(), aotInfos.end(), ProductConfigHelper::findDeviceAcronymForRelease(aotInfo.release));
        if (!hasDeviceAcronym) {
            deviceInfo = &aotInfo;
            break;
        }
    }
    if (deviceInfo == nullptr) {
        GTEST_SKIP();
    }

    for (const auto &aotInfo : aotInfos) {
        if (aotInfo.release == deviceInfo->release) {
            if (!aotInfo.rtlIdAcronyms.empty()) {
                expected.push_back(aotInfo.rtlIdAcronyms.front());
            }
        }
    }
    if (expected.empty()) {
        GTEST_SKIP();
    }

    auto it = std::find_if(AOT::releaseAcronyms.begin(), AOT::releaseAcronyms.end(), findAcronymForEnum(deviceInfo->release));

    auto got = NEO::getTargetProductsForFatbinary(it->first, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    deviceInfo->rtlIdAcronyms.clear();

    got = NEO::getTargetProductsForFatbinary(it->first, oclocArgHelperWithoutInput.get());
    EXPECT_NE(got, expected);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenReleaseWhichHasDeviceAcronymWhenGetTargetProductsForFatbinaryThenCorrectResultIsReturned) {
    auto &aotInfos = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    DeviceAotInfo *deviceInfo = nullptr;
    std::vector<ConstStringRef> expected{};

    for (auto &aotInfo : aotInfos) {
        auto hasDeviceAcronym = std::any_of(aotInfos.begin(), aotInfos.end(), ProductConfigHelper::findDeviceAcronymForRelease(aotInfo.release));
        if (hasDeviceAcronym && !aotInfo.deviceAcronyms.empty()) {
            deviceInfo = &aotInfo;
            break;
        }
    }
    if (deviceInfo == nullptr) {
        GTEST_SKIP();
    }
    for (const auto &aotInfo : aotInfos) {
        if (aotInfo.release == deviceInfo->release) {
            if (!aotInfo.deviceAcronyms.empty()) {
                expected.push_back(aotInfo.deviceAcronyms.front());
            }
        }
    }
    auto it = std::find_if(AOT::releaseAcronyms.begin(), AOT::releaseAcronyms.end(), findAcronymForEnum(deviceInfo->release));

    auto got = NEO::getTargetProductsForFatbinary(it->first, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    deviceInfo->deviceAcronyms.clear();

    got = NEO::getTargetProductsForFatbinary(it->first, oclocArgHelperWithoutInput.get());
    EXPECT_NE(got, expected);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenReleaseWithDeviceAcronymWhenItIsNoLongerExistThenGetRtlIdAcronyms) {
    auto &aotInfos = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    DeviceAotInfo *deviceInfo = nullptr;
    std::vector<ConstStringRef> expected{};

    for (auto &aotInfo : aotInfos) {
        auto hasDeviceAcronym = std::any_of(aotInfos.begin(), aotInfos.end(), ProductConfigHelper::findDeviceAcronymForRelease(aotInfo.release));
        if (hasDeviceAcronym && !aotInfo.deviceAcronyms.empty()) {
            deviceInfo = &aotInfo;
            break;
        }
    }
    if (deviceInfo == nullptr) {
        GTEST_SKIP();
    }
    for (auto &aotInfo : aotInfos) {
        if (aotInfo.release == deviceInfo->release) {
            aotInfo.deviceAcronyms.clear();
            if (!aotInfo.rtlIdAcronyms.empty()) {
                expected.push_back(aotInfo.rtlIdAcronyms.front());
            }
        }
    }
    auto it = std::find_if(AOT::releaseAcronyms.begin(), AOT::releaseAcronyms.end(), findAcronymForEnum(deviceInfo->release));

    EXPECT_FALSE(std::any_of(aotInfos.begin(), aotInfos.end(), ProductConfigHelper::findDeviceAcronymForRelease(deviceInfo->release)));
    auto got = NEO::getTargetProductsForFatbinary(it->first, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOnlyRtlIdAcronymsWhenGetProductsAcronymsForReleaseThenCorrectResulstsAreReturned) {
    auto &aotInfos = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    DeviceAotInfo *deviceInfo = nullptr;
    std::vector<ConstStringRef> expected{}, got{};

    for (auto &aotInfo : aotInfos) {
        aotInfo.deviceAcronyms.clear();
        if (!aotInfo.rtlIdAcronyms.empty()) {
            deviceInfo = &aotInfo;
        }
    }
    if (deviceInfo == nullptr) {
        GTEST_SKIP();
    }
    for (const auto &aotInfo : aotInfos) {
        if (deviceInfo->release == aotInfo.release && !aotInfo.rtlIdAcronyms.empty()) {
            expected.push_back(aotInfo.rtlIdAcronyms.front());
        }
    }

    EXPECT_FALSE(std::any_of(aotInfos.begin(), aotInfos.end(), ProductConfigHelper::findDeviceAcronymForRelease(deviceInfo->release)));
    getProductsAcronymsForTarget<AOT::RELEASE>(got, deviceInfo->release, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeFromFamilyWithoutDashesWhenGetProductsForFatBinaryThenCorrectAcronymsAreReturned) {
    auto acronyms = prepareFamiliesWithoutDashes(oclocArgHelperWithoutInput.get());
    if (acronyms.empty()) {
        GTEST_SKIP();
    }
    std::vector<ConstStringRef> expected{};

    auto familyFromId = oclocArgHelperWithoutInput->productConfigHelper->getFamilyFromDeviceName(acronyms[0]);
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

    const auto family = (enabledFamiliesAcronyms.end() - 3)->str();
    std::vector<ConstStringRef> expected{};

    auto familyFromId = oclocArgHelperWithoutInput->productConfigHelper->getFamilyFromDeviceName(family);
    auto familyToId = AOT::FAMILY_MAX;
    while (familyFromId < familyToId) {
        getProductsAcronymsForTarget(expected, familyFromId, oclocArgHelperWithoutInput.get());
        familyFromId = static_cast<AOT::FAMILY>(static_cast<unsigned int>(familyFromId) + 1);
    }

    std::string familiesTarget = family + ":";
    auto got = NEO::getTargetProductsForFatbinary(familiesTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        familiesTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryProductAcronymsTests, givenOpenRangeToFamilyWhenFatBinaryBuildIsInvokedThenSuccessIsReturned) {
    if (enabledFamiliesAcronyms.size() < 3) {
        GTEST_SKIP();
    }

    const auto &family = enabledFamiliesAcronyms.at(2);
    std::vector<ConstStringRef> expected{};

    auto familyFromId = static_cast<AOT::FAMILY>(static_cast<unsigned int>(AOT::UNKNOWN_FAMILY) + 1);
    auto familyToId = oclocArgHelperWithoutInput->productConfigHelper->getFamilyFromDeviceName(family.str());

    while (familyFromId <= familyToId && familyFromId < AOT::FAMILY_MAX) {
        getProductsAcronymsForTarget(expected, familyFromId, oclocArgHelperWithoutInput.get());
        familyFromId = static_cast<AOT::FAMILY>(static_cast<unsigned int>(familyFromId) + 1);
    }

    std::string familiesTarget = ":" + family.str();
    auto got = NEO::getTargetProductsForFatbinary(familiesTarget, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(got, expected);

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);
    std::stringstream resString;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        familiesTarget};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = buildFatBinary(argv, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(retVal, OCLOC_SUCCESS);

    for (const auto &product : expected) {
        resString << "Build succeeded for : " << product.str() + ".\n";
    }

    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST_F(OclocFatBinaryTest, givenSpirvInputWhenFatBinaryIsRequestedThenArchiveContainsOptions) {
    const auto devices = prepareTwoDevices(&mockArgHelper);
    if (devices.empty()) {
        GTEST_SKIP();
    }

    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedtedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedtedFiles.begin(), expectedtedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedtedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

    char data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    MockCompilerDebugVars igcDebugVars(gEnvironment->igcDebugVars);
    igcDebugVars.binaryToReturn = data;
    igcDebugVars.binaryToReturnSize = sizeof(data);
    NEO::setIgcDebugVars(igcDebugVars);

    std::string dummyOptions = "-dummy-option ";
    const std::vector<std::string> args = {
        "ocloc",
        "-output",
        outputArchiveName,
        "-file",
        spirvFilename,
        "-output_no_suffix",
        "-spirv_input",
        "-options",
        dummyOptions,
        "-device",
        devices};

    mockArgHelper.getPrinterRef().setSuppressMessages(true);
    const auto buildResult = buildFatBinary(args, &mockArgHelper);
    ASSERT_EQ(OCLOC_SUCCESS, buildResult);
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

    const auto isOptionSection = [](const auto &section) {
        return section.header && section.header->type == Elf::SHT_OPENCL_OPTIONS;
    };

    const auto optionSectionIt = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(), isOptionSection);
    ASSERT_NE(elf.sectionHeaders.end(), optionSectionIt);

    ASSERT_EQ(dummyOptions.size(), optionSectionIt->header->size);
    const auto isSpirvDataEqualsInputFileData = std::memcmp(dummyOptions.data(), optionSectionIt->data.begin(), dummyOptions.size()) == 0;
    EXPECT_TRUE(isSpirvDataEqualsInputFileData);
    NEO::setIgcDebugVars(gEnvironment->igcDebugVars);
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

    mockArgHelper.getPrinterRef().setSuppressMessages(true);
    const auto buildResult = buildFatBinary(args, &mockArgHelper);
    ASSERT_EQ(OCLOC_SUCCESS, buildResult);
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

    StreamCapture capture;
    capture.captureStdout();
    const auto result = buildFatBinary(args, &mockArgHelper);
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, result);

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

        StreamCapture capture;
        capture.captureStdout();
        const auto result = buildFatBinary(args, &mockArgHelper);
        const auto output{capture.getCapturedStdout()};

        EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, result);

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

        mockArgHelper.getPrinterRef().setSuppressMessages(true);
        const auto buildResult = buildFatBinary(args, &mockArgHelper);
        ASSERT_EQ(OCLOC_SUCCESS, buildResult);
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

    mockArgHelper.getPrinterRef().setSuppressMessages(true);
    const auto buildResult = buildFatBinary(args, &mockArgHelper);
    ASSERT_EQ(OCLOC_SUCCESS, buildResult);

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

    mockArgHelper.getPrinterRef().setSuppressMessages(true);
    const auto buildResult = buildFatBinary(args, &mockArgHelper);
    ASSERT_EQ(OCLOC_SUCCESS, buildResult);
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

    mockArgHelper.getPrinterRef().setSuppressMessages(true);
    const auto buildResult = buildFatBinary(args, &mockArgHelper);
    ASSERT_EQ(OCLOC_SUCCESS, buildResult);
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
    std::string dummyOptions{"-cl-opt-disable "};
    mockArgHelperFilesMap[emptyFile] = "";
    mockArgHelper.shouldLoadDataFromFileReturnZeroSize = true;

    StreamCapture capture;
    capture.captureStdout();
    const auto errorCode{appendGenericIr(ar, emptyFile, &mockArgHelper, dummyOptions)};
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_INVALID_FILE, errorCode);
    EXPECT_EQ("Error! Couldn't read input file!\n", output);
}

TEST_F(OclocFatBinaryTest, givenInvalidIrFileWhenAppendingGenericIrThenInvalidFileIsReturned) {
    Ar::ArEncoder ar;
    std::string dummyFile{"dummy_file.spv"};
    std::string dummyOptions{"-cl-opt-disable "};
    mockArgHelperFilesMap[dummyFile] = "This is not IR!";

    StreamCapture capture;
    capture.captureStdout();
    const auto errorCode{appendGenericIr(ar, dummyFile, &mockArgHelper, dummyOptions)};
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_INVALID_FILE, errorCode);

    const auto expectedErrorMessage{"Error! Input file is not in supported generic IR format! "
                                    "Currently supported format is SPIR-V.\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OclocTest, givenPreviousCompilationErrorWhenBuildingFatbinaryForTargetThenNothingIsDoneAndErrorIsReturned) {
    const std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.argHelper->getPrinterRef().setSuppressMessages(true);
    mockOfflineCompiler.initialize(argv.size(), argv);

    // We expect that nothing is done and error is returned.
    // Therefore, if offline compiler is used, ensure that it just returns error code,
    // which is different than expected one.
    mockOfflineCompiler.buildReturnValue = OCLOC_SUCCESS;

    Ar::ArEncoder ar;
    const std::string pointerSize{"32"};
    const auto mockArgHelper = mockOfflineCompiler.uniqueHelper.get();
    const auto deviceConfig = getDeviceConfig(mockOfflineCompiler, mockArgHelper);

    const int previousReturnValue{OCLOC_INVALID_FILE};
    const auto buildResult = buildFatBinaryForTarget(previousReturnValue, argv, pointerSize, ar, &mockOfflineCompiler, mockArgHelper, deviceConfig);

    EXPECT_EQ(OCLOC_INVALID_FILE, buildResult);
    EXPECT_EQ(0, mockOfflineCompiler.buildCalledCount);
}

TEST_F(OclocTest, givenPreviousCompilationSuccessAndFailingBuildWhenBuildingFatbinaryForTargetThenCompilationIsInvokedAndErrorLogIsPrinted) {
    const std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    mockOfflineCompiler.buildReturnValue = OCLOC_INVALID_FILE;

    Ar::ArEncoder ar;
    const std::string pointerSize{"32"};
    const auto mockArgHelper = mockOfflineCompiler.uniqueHelper.get();
    const auto deviceConfig = getDeviceConfig(mockOfflineCompiler, mockArgHelper);

    StreamCapture capture;
    capture.captureStdout();
    const int previousReturnValue{OCLOC_SUCCESS};
    const auto buildResult = buildFatBinaryForTarget(previousReturnValue, argv, pointerSize, ar, &mockOfflineCompiler, mockArgHelper, deviceConfig);
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_INVALID_FILE, buildResult);
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

TEST_F(OclocTest, givenNonEmptyBuildLogWhenBuildingFatbinaryForTargetThenBuildLogIsPrinted) {
    using namespace std::string_literals;

    const std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    const char buildWarning[] = "warning: This is a build log!";
    mockOfflineCompiler.updateBuildLog(buildWarning, sizeof(buildWarning));
    mockOfflineCompiler.buildReturnValue = OCLOC_SUCCESS;

    // Dummy value
    mockOfflineCompiler.elfBinary = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    Ar::ArEncoder ar;
    const std::string pointerSize{"32"};
    const auto mockArgHelper = mockOfflineCompiler.uniqueHelper.get();
    const auto deviceConfig = getDeviceConfig(mockOfflineCompiler, mockArgHelper);

    StreamCapture capture;
    capture.captureStdout();
    const int previousReturnValue{OCLOC_SUCCESS};
    const auto buildResult = buildFatBinaryForTarget(previousReturnValue, argv, pointerSize, ar, &mockOfflineCompiler, mockArgHelper, deviceConfig);
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_SUCCESS, buildResult);
    EXPECT_EQ(1, mockOfflineCompiler.buildCalledCount);

    const std::string expectedOutput{buildWarning + "\nBuild succeeded for : "s + deviceConfig + ".\n"s};
    EXPECT_EQ(expectedOutput, output);
}

TEST_F(OclocTest, givenNonEmptyBuildLogWhenBuildingFatbinaryForTargetThenBuildLogIsNotPrinted) {
    StreamCapture capture;
    capture.captureStdout();

    const std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    const auto mockArgHelper = mockOfflineCompiler.uniqueHelper.get();
    const auto deviceConfig = getDeviceConfig(mockOfflineCompiler, mockArgHelper);

    const char buildWarning[] = "Warning: this is a build log!";
    mockOfflineCompiler.updateBuildLog(buildWarning, sizeof(buildWarning));
    mockOfflineCompiler.buildReturnValue = OCLOC_SUCCESS;

    mockOfflineCompiler.elfBinary = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    Ar::ArEncoder ar;
    const std::string pointerSize{"32"};

    const int previousReturnValue{OCLOC_SUCCESS};
    buildFatBinaryForTarget(previousReturnValue, argv, pointerSize, ar, &mockOfflineCompiler, mockArgHelper, deviceConfig);
    const auto output{capture.getCapturedStdout()};

    EXPECT_TRUE(output.empty()) << output;
}

TEST_F(OclocTest, givenListOfDeprecatedAcronymsThenUseThemAsIs) {
    ProductConfigHelper productConfigHelper;
    auto allDeperecatedAcronyms = productConfigHelper.getDeprecatedAcronyms();
    if (allDeperecatedAcronyms.empty()) {
        return;
    }

    StreamCapture capture;
    capture.captureStdout();

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        allDeperecatedAcronyms[0].str()};
    int deviceArgIndex = 5;

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    const auto mockArgHelper = mockOfflineCompiler.uniqueHelper.get();
    const auto deviceConfig = getDeviceConfig(mockOfflineCompiler, mockArgHelper);

    mockOfflineCompiler.buildReturnValue = OCLOC_SUCCESS;

    Ar::ArEncoder ar;
    const std::string pointerSize{"64"};

    const int previousReturnValue{OCLOC_SUCCESS};
    for (const auto &product : allDeperecatedAcronyms) {
        argv[deviceArgIndex] = product.str();

        auto retVal = buildFatBinaryForTarget(previousReturnValue, argv, pointerSize, ar, &mockOfflineCompiler, mockArgHelper, product.str());
        EXPECT_EQ(0, retVal);
    }
    const auto output{capture.getCapturedStdout()};
    EXPECT_TRUE(output.empty()) << output;

    auto encodedFatbin = ar.encode();
    std::string arError, arWarning;
    auto decodedAr = Ar::decodeAr(encodedFatbin, arError, arWarning);
    EXPECT_TRUE(arError.empty()) << arError;
    EXPECT_TRUE(arWarning.empty()) << arWarning;
    std::unordered_set<std::string> entryNames;
    for (const auto &entry : decodedAr.files) {
        entryNames.insert(entry.fileName.str());
    }

    for (const auto &deprecatedAcronym : allDeperecatedAcronyms) {
        auto expectedFName = (pointerSize + ".") + deprecatedAcronym.data();
        EXPECT_EQ(1U, entryNames.count(expectedFName)) << deprecatedAcronym.data() << "not found in [" << ConstStringRef(",").join(entryNames) << "]";
    }
}

TEST_F(OclocTest, givenListOfGenericAcronymsThenUseThemAsIs) {
    ProductConfigHelper productConfigHelper;
    std::vector<NEO::ConstStringRef> genericAcronyms{};

    for (const auto &genericIdEntry : AOT::genericIdAcronyms) {
        genericAcronyms.push_back(genericIdEntry.first);
    }
    if (genericAcronyms.empty()) {
        GTEST_SKIP();
    }

    StreamCapture capture;
    capture.captureStdout();

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        genericAcronyms[0].str()};
    int deviceArgIndex = 5;

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    const auto mockArgHelper = mockOfflineCompiler.uniqueHelper.get();
    const auto deviceConfig = getDeviceConfig(mockOfflineCompiler, mockArgHelper);

    mockOfflineCompiler.buildReturnValue = OCLOC_SUCCESS;

    Ar::ArEncoder ar;
    const std::string pointerSize{"64"};

    const int previousReturnValue{OCLOC_SUCCESS};
    for (const auto &product : genericAcronyms) {
        argv[deviceArgIndex] = product.str();

        auto retVal = buildFatBinaryForTarget(previousReturnValue, argv, pointerSize, ar, &mockOfflineCompiler, mockArgHelper, product.str());
        EXPECT_EQ(0, retVal);
    }
    const auto output{capture.getCapturedStdout()};
    EXPECT_TRUE(output.empty()) << output;

    auto encodedFatbin = ar.encode();
    std::string arError, arWarning;
    auto decodedAr = Ar::decodeAr(encodedFatbin, arError, arWarning);
    EXPECT_TRUE(arError.empty()) << arError;
    EXPECT_TRUE(arWarning.empty()) << arWarning;
    std::unordered_set<std::string> entryNames;
    for (const auto &entry : decodedAr.files) {
        entryNames.insert(entry.fileName.str());
    }

    for (const auto &expectedAcronym : genericAcronyms) {
        auto expectedName = (pointerSize + ".") + expectedAcronym.data();
        EXPECT_EQ(1U, entryNames.count(expectedName)) << expectedAcronym.data() << "not found in [" << ConstStringRef(",").join(entryNames) << "]";
    }
}

TEST_F(OclocTest, givenQuietModeWhenBuildingFatbinaryForTargetThenNothingIsPrinted) {
    using namespace std::string_literals;

    const std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-q",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    // Dummy value
    mockOfflineCompiler.elfBinary = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    mockOfflineCompiler.buildReturnValue = OCLOC_SUCCESS;

    Ar::ArEncoder ar;
    const std::string pointerSize{"32"};
    const auto mockArgHelper = mockOfflineCompiler.uniqueHelper.get();
    const auto deviceConfig = getDeviceConfig(mockOfflineCompiler, mockArgHelper);

    StreamCapture capture;
    capture.captureStdout();
    const int previousReturnValue{OCLOC_SUCCESS};
    const auto buildResult = buildFatBinaryForTarget(previousReturnValue, argv, pointerSize, ar, &mockOfflineCompiler, mockArgHelper, deviceConfig);
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_SUCCESS, buildResult);
    EXPECT_EQ(1, mockOfflineCompiler.buildCalledCount);

    EXPECT_TRUE(output.empty()) << output;
}

TEST_F(OclocTest, WhenDeviceArgIsPresentReturnsCorrectIndex) {
    std::vector<std::string> args = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};
    EXPECT_EQ(4, getDeviceArgValueIdx(args));
}

TEST_F(OclocTest, WhenDeviceArgIsLastReturnsMinusOne) {
    std::vector<std::string> args = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device"};
    EXPECT_EQ(-1, getDeviceArgValueIdx(args));
}

TEST_F(OclocTest, WhenDeviceArgIsAbsentReturnsMinusOne) {
    std::vector<std::string> args = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str()};
    EXPECT_EQ(-1, getDeviceArgValueIdx(args));
}

TEST_F(OclocTest, WhenArgsAreEmptyReturnsMinusOne) {
    std::vector<std::string> args = {};
    EXPECT_EQ(-1, getDeviceArgValueIdx(args));
}

TEST_P(OclocFatbinaryPerProductTests, givenReleaseWhenGetTargetProductsForFarbinaryThenCorrectAcronymsAreReturned) {
    auto aotInfos = argHelper->productConfigHelper->getDeviceAotInfo();
    std::vector<NEO::ConstStringRef> expected{};
    auto releaseValue = argHelper->productConfigHelper->getReleaseFromDeviceName(release);
    auto hasDeviceAcronym = std::any_of(aotInfos.begin(), aotInfos.end(), ProductConfigHelper::findDeviceAcronymForRelease(releaseValue));

    for (const auto &aotInfo : aotInfos) {
        if (aotInfo.hwInfo->platform.eProductFamily == productFamily && aotInfo.release == releaseValue) {
            if (hasDeviceAcronym) {
                if (!aotInfo.deviceAcronyms.empty()) {
                    expected.push_back(aotInfo.deviceAcronyms.front());
                }
            } else {
                if (!aotInfo.rtlIdAcronyms.empty()) {
                    expected.push_back(aotInfo.rtlIdAcronyms.front());
                }
            }
        }
    }

    auto got = NEO::getTargetProductsForFatbinary(release, argHelper.get());
    EXPECT_EQ(got, expected);
}
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(OclocFatbinaryPerProductTests);
} // namespace NEO