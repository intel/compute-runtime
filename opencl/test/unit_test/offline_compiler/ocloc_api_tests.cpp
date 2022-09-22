/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/decoder/helper.h"
#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_concat.h"
#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/offline_compiler/source/queries.h"
#include "shared/offline_compiler/source/utilities/get_git_version_info.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "environment.h"
#include "gtest/gtest.h"
#include "hw_cmds_default.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

extern Environment *gEnvironment;

using namespace std::string_literals;

void mockedAbortOclocExecution(int errorCode) {
    throw std::runtime_error{"mockedAbortOclocExecution() called with error code = " + std::to_string(errorCode)};
}

TEST(OclocApiTests, WhenOclocVersionIsCalledThenCurrentOclocVersionIsReturned) {
    EXPECT_EQ(ocloc_version_t::OCLOC_VERSION_CURRENT, oclocVersion());
}

TEST(OclocApiTests, WhenGoodArgsAreGivenThenSuccessIsReturned) {
    std::string clFileName(clFiles + "copybuffer.cl");
    const char *argv[] = {
        "ocloc",
        "-file",
        clFileName.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);
    EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file test_files/copybuffer.cl -device "s + argv[4]));
    EXPECT_NE(std::string::npos, output.find("Build succeeded.\n"));
}

TEST(OclocApiTests, GivenQuietModeAndValidArgumentsWhenRunningOclocThenSuccessIsReturnedAndBuildSucceededMessageIsNotPrinted) {
    std::string clFileName(clFiles + "copybuffer.cl");
    const char *argv[] = {
        "ocloc",
        "-file",
        clFileName.c_str(),
        "-q",
        "-device",
        gEnvironment->devicePrefix.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(NEO::OclocErrorCode::SUCCESS, retVal);
    EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file test_files/copybuffer.cl -device "s + argv[4]));
    EXPECT_EQ(std::string::npos, output.find("Build succeeded.\n"));
}

TEST(OclocApiTests, GivenNeoRevisionQueryWhenQueryingThenNeoRevisionIsReturned) {
    uint32_t numOutputs;
    uint64_t *lenOutputs;
    uint8_t **dataOutputs;
    char **nameOutputs;
    const char *argv[] = {
        "ocloc",
        "query",
        NEO::Queries::queryNeoRevision.data()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);
    EXPECT_EQ(numOutputs, 2u);

    int queryOutputIndex = -1;
    for (uint32_t i = 0; i < numOutputs; ++i) {
        if (strcmp(NEO::Queries::queryNeoRevision.data(), nameOutputs[i]) == 0) {
            queryOutputIndex = i;
        }
    }
    ASSERT_NE(-1, queryOutputIndex);
    NEO::ConstStringRef queryOutput(reinterpret_cast<const char *>(dataOutputs[queryOutputIndex]),
                                    static_cast<size_t>(lenOutputs[queryOutputIndex]));
    EXPECT_STREQ(NEO::getRevision().c_str(), queryOutput.data());

    oclocFreeOutput(&numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
}

TEST(OclocApiTests, GivenOclDriverVersionQueryWhenQueryingThenNeoRevisionIsReturned) {
    uint32_t numOutputs;
    uint64_t *lenOutputs;
    uint8_t **dataOutputs;
    char **nameOutputs;
    const char *argv[] = {
        "ocloc",
        "query",
        NEO::Queries::queryOCLDriverVersion.data()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);
    EXPECT_EQ(numOutputs, 2u);

    int queryOutputIndex = -1;
    for (uint32_t i = 0; i < numOutputs; ++i) {
        if (strcmp(NEO::Queries::queryOCLDriverVersion.data(), nameOutputs[i]) == 0) {
            queryOutputIndex = i;
        }
    }
    ASSERT_NE(-1, queryOutputIndex);
    NEO::ConstStringRef queryOutput(reinterpret_cast<const char *>(dataOutputs[queryOutputIndex]),
                                    static_cast<size_t>(lenOutputs[queryOutputIndex]));
    EXPECT_STREQ(NEO::getOclDriverVersion().c_str(), queryOutput.data());

    oclocFreeOutput(&numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
}

TEST(OclocApiTests, GivenNoQueryWhenQueryingThenErrorIsReturned) {
    const char *argv[] = {
        "ocloc",
        "query"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);
    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(retVal, NEO::OclocErrorCode::INVALID_COMMAND_LINE);
    EXPECT_STREQ("Error: Invalid command line. Expected ocloc query <argument>", output.c_str());
}

TEST(OclocApiTests, GivenInvalidQueryWhenQueryingThenErrorIsReturned) {
    const char *argv[] = {
        "ocloc",
        "query",
        "unknown_query"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);
    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(retVal, NEO::OclocErrorCode::INVALID_COMMAND_LINE);
    EXPECT_STREQ("Error: Invalid command line. Unknown argument unknown_query.", output.c_str());
}

TEST(OclocApiTests, givenNoAcronymWhenIdsCommandIsInvokeThenErrorIsReported) {
    const char *argv[] = {
        "ocloc",
        "ids"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);
    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(retVal, NEO::OclocErrorCode::INVALID_COMMAND_LINE);
    EXPECT_STREQ("Error: Invalid command line. Expected ocloc ids <acronym>.\n", output.c_str());
}

TEST(OclocApiTests, givenUnknownAcronymWhenIdsCommandIsInvokeThenErrorIsReported) {
    const char *argv[] = {
        "ocloc",
        "ids",
        "unk"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);
    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(retVal, NEO::OclocErrorCode::INVALID_COMMAND_LINE);
    EXPECT_STREQ("Error: Invalid command line. Unknown acronym unk.\n", output.c_str());
}

TEST(OclocApiTests, WhenGoodFamilyNameIsProvidedThenSuccessIsReturned) {
    std::string clFileName(clFiles + "copybuffer.cl");
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allSupportedDeviceConfigs = argHelper->productConfigHelper->getDeviceAotInfo();
    if (allSupportedDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    std::string family("");
    for (const auto &config : allSupportedDeviceConfigs) {
        if (config.hwInfo->platform.eProductFamily == NEO::DEFAULT_PLATFORM::hwInfo.platform.eProductFamily) {
            family = ProductConfigHelper::getAcronymForAFamily(config.family).str();
            break;
        }
    }
    if (family.empty()) {
        GTEST_SKIP();
    }

    const char *argv[] = {
        "ocloc",
        "-file",
        clFileName.c_str(),
        "-device",
        family.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);
    EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file " + clFileName + " -device " + family));
}

TEST(OclocApiTests, WhenArgsWithMissingFileAreGivenThenErrorMessageIsProduced) {
    const char *argv[] = {
        "ocloc",
        "-q",
        "-file",
        "test_files/IDoNotExist.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(retVal, NEO::OclocErrorCode::INVALID_FILE);
    EXPECT_NE(std::string::npos, output.find("Command was: ocloc -q -file test_files/IDoNotExist.cl -device "s + argv[5]));
}

TEST(OclocApiTests, givenInputOptionsAndInternalOptionsWhenCmdlineIsPrintedThenBothAreInQuotes) {
    const char *argv[] = {
        "ocloc",
        "-q",
        "-file",
        "test_files/IDoNotExist.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-options", "-D DEBUG -cl-kernel-arg-info", "-internal_options", "-internalOption1 -internal-option-2"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(retVal, NEO::OclocErrorCode::SUCCESS);
    EXPECT_TRUE(output.find("Command was: ocloc -q -file test_files/IDoNotExist.cl -device " +
                            gEnvironment->devicePrefix +
                            " -options \"-D DEBUG -cl-kernel-arg-info\" -internal_options \"-internalOption1 -internal-option-2\"") != std::string::npos);

    size_t quotesCount = std::count(output.begin(), output.end(), '\"');
    EXPECT_EQ(quotesCount, 4u);
}

TEST(OclocApiTests, givenInputOptionsCalledOptionsWhenCmdlineIsPrintedThenQuotesAreCorrect) {
    const char *argv[] = {
        "ocloc",
        "-q",
        "-file",
        "test_files/IDoNotExist.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-options", "-options", "-internal_options", "-internalOption"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(retVal, NEO::OclocErrorCode::SUCCESS);
    EXPECT_TRUE(output.find("Command was: ocloc -q -file test_files/IDoNotExist.cl -device " +
                            gEnvironment->devicePrefix +
                            " -options \"-options\" -internal_options \"-internalOption\"") != std::string::npos);

    size_t quotesCount = std::count(output.begin(), output.end(), '\"');
    EXPECT_EQ(quotesCount, 4u);
}

TEST(OclocApiTests, givenInvalidInputOptionsAndInternalOptionsFilesWhenCmdlineIsPrintedThenTheyArePrinted) {
    ASSERT_TRUE(fileExists(clFiles + "shouldfail.cl"));
    ASSERT_TRUE(fileExists(clFiles + "shouldfail_options.txt"));
    ASSERT_TRUE(fileExists(clFiles + "shouldfail_internal_options.txt"));

    std::string clFileName(clFiles + "shouldfail.cl");
    const char *argv[] = {
        "ocloc",
        "-q",
        "-file",
        clFileName.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(retVal, NEO::OclocErrorCode::SUCCESS);

    EXPECT_TRUE(output.find("Compiling options read from file were:\n"
                            "-shouldfailOptions") != std::string::npos);

    EXPECT_TRUE(output.find("Internal options read from file were:\n"
                            "-shouldfailInternalOptions") != std::string::npos);
}

TEST(OclocApiTests, GivenInvalidOptionsAndInternalOptionsCommandArgumentsWhenCmdlineIsPrintedThenTheyAreNotPrinted) {
    std::string clFileName(clFiles + "shouldfail.cl");

    ASSERT_TRUE(fileExists(clFileName));

    const char *argv[] = {
        "ocloc",
        "-q",
        "-options",
        "-invalid_option",
        "-internal_options",
        "-invalid_internal_option",
        "-file",
        clFileName.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(NEO::OclocErrorCode::SUCCESS, retVal);

    EXPECT_FALSE(output.find("Compiling options read from file were:\n"
                             "-shouldfailOptions") != std::string::npos);

    EXPECT_FALSE(output.find("Internal options read from file were:\n"
                             "-shouldfailInternalOptions") != std::string::npos);
}

TEST(OclocApiTests, givenInvalidOclocOptionsFileWhenCmdlineIsPrintedThenTheyArePrinted) {
    ASSERT_TRUE(fileExists(clFiles + "valid_kernel.cl"));
    ASSERT_TRUE(fileExists(clFiles + "valid_kernel_ocloc_options.txt"));
    std::string clFileName(clFiles + "valid_kernel.cl");

    const char *argv[] = {
        "ocloc",
        "-q",
        "-file",
        clFileName.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(retVal, NEO::OclocErrorCode::SUCCESS);

    EXPECT_TRUE(output.find("Failed with ocloc options from file:\n"
                            "-invalid_ocloc_option") != std::string::npos);
    EXPECT_FALSE(output.find("Building with ocloc options:") != std::string::npos);
}

TEST(OclocApiTests, GivenIncludeHeadersWhenCompilingThenPassesToFclHeadersPackedAsElf) {
    auto prevFclDebugVars = NEO::getFclDebugVars();
    auto debugVars = prevFclDebugVars;
    std::string receivedInput;
    debugVars.receivedInput = &receivedInput;
    setFclDebugVars(debugVars);

    const char *argv[] = {
        "ocloc",
        "-file",
        "main.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    const char *headerA = R"===(
       void foo() {} 
)===";

    const char *headerB = R"===(
       void bar() {} 
)===";

    const char *main = R"===(
#include "includeA.h"
#include "includeB.h"

__kernel void k(){
    foo();
    bar();
}
)===";

    const char *sourcesNames[] = {"main.cl"};
    const uint8_t *sources[] = {reinterpret_cast<const uint8_t *>(main)};
    const uint64_t sourcesLen[] = {strlen(main) + 1};

    const char *headersNames[] = {"includeA.h", "includeB.h"};
    const uint8_t *headers[] = {reinterpret_cast<const uint8_t *>(headerA), reinterpret_cast<const uint8_t *>(headerB)};
    const uint64_t headersLen[] = {strlen(headerA) + 1, strlen(headerB) + 1};

    uint32_t numOutputs = 0U;
    uint8_t **outputs = nullptr;
    uint64_t *outputsLen = nullptr;
    char **ouputsNames = nullptr;

    oclocInvoke(argc, argv,
                1, sources, sourcesLen, sourcesNames,
                2, headers, headersLen, headersNames,
                &numOutputs, &outputs, &outputsLen, &ouputsNames);

    NEO::setFclDebugVars(prevFclDebugVars);

    std::string decodeErr, decodeWarn;
    ArrayRef<const uint8_t> rawElf(reinterpret_cast<const uint8_t *>(receivedInput.data()), receivedInput.size());
    auto elf = NEO::Elf::decodeElf(rawElf, decodeErr, decodeWarn);
    ASSERT_NE(nullptr, elf.elfFileHeader) << decodeWarn << " " << decodeErr;
    EXPECT_EQ(NEO::Elf::ET_OPENCL_SOURCE, elf.elfFileHeader->type);
    using SectionT = std::remove_reference_t<decltype(*elf.sectionHeaders.begin())>;
    const SectionT *sourceSection, *headerASection, *headerBSection;

    ASSERT_NE(NEO::Elf::SHN_UNDEF, elf.elfFileHeader->shStrNdx);
    auto sectionNamesSection = elf.sectionHeaders.begin() + elf.elfFileHeader->shStrNdx;
    auto elfStrings = sectionNamesSection->data.toArrayRef<const char>();
    for (const auto &section : elf.sectionHeaders) {
        if (NEO::Elf::SHT_OPENCL_SOURCE == section.header->type) {
            sourceSection = &section;
        } else if (NEO::Elf::SHT_OPENCL_HEADER == section.header->type) {
            auto sectionName = elfStrings.begin() + section.header->name;
            if (0 == strcmp("includeA.h", sectionName)) {
                headerASection = &section;
            } else if (0 == strcmp("includeB.h", sectionName)) {
                headerBSection = &section;
            } else {
                EXPECT_FALSE(true) << sectionName;
            }
        }
    }

    ASSERT_NE(nullptr, sourceSection);
    EXPECT_EQ(sourcesLen[0], sourceSection->data.size());
    EXPECT_STREQ(main, reinterpret_cast<const char *>(sourceSection->data.begin()));

    ASSERT_NE(nullptr, headerASection);
    EXPECT_EQ(sourcesLen[0], sourceSection->data.size());
    EXPECT_STREQ(headerA, reinterpret_cast<const char *>(headerASection->data.begin()));

    ASSERT_NE(nullptr, headerBSection);
    EXPECT_EQ(sourcesLen[0], sourceSection->data.size());
    EXPECT_STREQ(headerB, reinterpret_cast<const char *>(headerBSection->data.begin()));
}

TEST(OclocApiTests, GivenHelpParameterWhenDecodingThenHelpMsgIsPrintedAndSuccessIsReturned) {
    const char *argv[] = {
        "ocloc",
        "disasm",
        "--help"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(output.empty());
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);
}

TEST(OclocApiTests, GivenNonExistingFileWhenDecodingThenAbortIsCalled) {
    VariableBackup oclocAbortBackup{&abortOclocExecution, &mockedAbortOclocExecution};

    const char *argv[] = {
        "ocloc",
        "disasm",
        "-file",
        "nonexistent_file_that_should_fail",
        "-dump",
        "test_files/created"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    const auto retVal = oclocInvoke(argc, argv,
                                    0, nullptr, nullptr, nullptr,
                                    0, nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr, nullptr);
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_NE(std::string::npos, output.find("mockedAbortOclocExecution() called with error code = 1"));
    EXPECT_EQ(-1, retVal);
}

TEST(OclocApiTests, GivenMissingFileNameWhenDecodingThenErrorIsReturned) {
    const char *argv[] = {
        "ocloc",
        "disasm",
        "-file"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    const auto retVal = oclocInvoke(argc, argv,
                                    0, nullptr, nullptr, nullptr,
                                    0, nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr, nullptr);
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_NE(std::string::npos, output.find("Unknown argument -file\n"));
    EXPECT_EQ(-1, retVal);
}

TEST(OclocApiTests, GivenHelpParameterWhenEncodingThenHelpMsgIsPrintedAndSuccessIsReturned) {
    const char *argv[] = {
        "ocloc",
        "asm",
        "--help"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(output.empty());
    EXPECT_EQ(retVal, NEO::OclocErrorCode::SUCCESS);
}

TEST(OclocApiTests, GivenMissingDumpFileNameWhenEncodingThenErrorIsReturned) {
    const char *argv[] = {
        "ocloc",
        "asm",
        "-dump"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    const auto retVal = oclocInvoke(argc, argv,
                                    0, nullptr, nullptr, nullptr,
                                    0, nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr, nullptr);
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_NE(std::string::npos, output.find("Unknown argument -dump\n"));
    EXPECT_EQ(-1, retVal);
}

TEST(OclocApiTests, GivenValidArgumentsAndMissingPtmFileWhenEncodingThenErrorIsReturned) {
    const char *argv[] = {
        "ocloc",
        "asm",
        "-dump",
        "test_files/dump",
        "-out",
        "test_files/binary_gen.bin"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    const auto retVal = oclocInvoke(argc, argv,
                                    0, nullptr, nullptr, nullptr,
                                    0, nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr, nullptr);
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_NE(std::string::npos, output.find("Error! Couldn't find PTM.txt"));
    EXPECT_EQ(-1, retVal);
}

TEST(OclocApiTests, GiveMultiCommandHelpArgumentsWhenInvokingOclocThenHelpIsPrinted) {
    const char *argv[] = {
        "ocloc",
        "multi",
        "--help"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    const auto retVal = oclocInvoke(argc, argv,
                                    0, nullptr, nullptr, nullptr,
                                    0, nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr, nullptr);
    const auto output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(output.empty());

    EXPECT_EQ(-1, retVal);
}

TEST(OclocApiTests, GivenNonexistentFileWhenValidateIsInvokedThenErrorIsPrinted) {
    const char *argv[] = {
        "ocloc",
        "validate",
        "-file",
        "some_special_nonexistent_file.gen"};
    unsigned int argc = sizeof(argv) / sizeof(argv[0]);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);

    const auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(-1, retVal);

    const std::string expectedErrorMessage{"Error : Input file missing : some_special_nonexistent_file.gen\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST(OclocApiTests, GivenCommandWithoutArgsWhenOclocIsInvokedThenHelpIsPrinted) {
    const char *argv[] = {
        "ocloc"};
    unsigned int argc = sizeof(argv) / sizeof(argv[0]);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);

    const auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(NEO::OclocErrorCode::SUCCESS, retVal);
    EXPECT_FALSE(output.empty());
}

TEST(OclocApiTests, GivenHelpArgumentWhenOclocIsInvokedThenHelpIsPrinted) {
    constexpr std::array flagsToTest = {"-h", "--help"};
    for (const auto helpFlag : flagsToTest) {
        const char *argv[] = {
            "ocloc",
            helpFlag};

        unsigned int argc = sizeof(argv) / sizeof(argv[0]);

        testing::internal::CaptureStdout();
        int retVal = oclocInvoke(argc, argv,
                                 0, nullptr, nullptr, nullptr,
                                 0, nullptr, nullptr, nullptr,
                                 nullptr, nullptr, nullptr, nullptr);

        const auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(NEO::OclocErrorCode::SUCCESS, retVal);
        EXPECT_FALSE(output.empty());
    }
}

TEST(OclocApiTests, GivenHelpParameterWhenLinkingThenHelpMsgIsPrintedAndSuccessIsReturned) {
    const char *argv[] = {
        "ocloc",
        "link",
        "--help"};
    unsigned int argc = sizeof(argv) / sizeof(argv[0]);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(output.empty());
    EXPECT_EQ(NEO::OclocErrorCode::SUCCESS, retVal);
}

TEST(OclocApiTests, GivenInvalidParameterWhenLinkingThenErrorIsReturned) {
    const char *argv[] = {
        "ocloc",
        "link",
        "--dummy_param"};
    unsigned int argc = sizeof(argv) / sizeof(argv[0]);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(NEO::OclocErrorCode::INVALID_COMMAND_LINE, retVal);

    const std::string expectedInitError{"Invalid option (arg 2): --dummy_param\n"};
    const std::string expectedExecuteError{"Error: Linker cannot be executed due to unsuccessful initialization!\n"};
    const std::string expectedErrorMessage = expectedInitError + expectedExecuteError;
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST(OclocApiTests, GivenInvalidCommandLineWhenConcatenatingThenErrorIsReturned) {
    const char *argv[] = {
        "ocloc",
        "concat"};
    unsigned int argc = sizeof(argv) / sizeof(argv[0]);
    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(NEO::OclocErrorCode::INVALID_COMMAND_LINE, retVal);
    const std::string emptyCommandLineError = "No files to concatenate were provided.\n";
    const std::string expectedErrorMessage = emptyCommandLineError + NEO::OclocConcat::helpMessage.str();
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST(OclocApiTests, GivenValidCommandLineAndFatBinariesWhenConcatenatingThenNewFatBinaryIsCreated) {
    std::vector<uint8_t> file1(32, 0x0);
    std::vector<uint8_t> file2(32, 0x10);
    const std::string file1Name = "file1";
    const std::string file2Name = "file2";

    std::vector<uint8_t> fatBinary1;
    {
        NEO::Ar::ArEncoder arEncoder(true);
        arEncoder.appendFileEntry(file1Name, ArrayRef<const uint8_t>::fromAny(file1.data(), file1.size()));
        fatBinary1 = arEncoder.encode();
    }
    std::vector<uint8_t> fatBinary2;
    {
        NEO::Ar::ArEncoder arEncoder(true);
        arEncoder.appendFileEntry(file2Name, ArrayRef<const uint8_t>::fromAny(file2.data(), file2.size()));
        fatBinary2 = arEncoder.encode();
    }

    const uint8_t *sourcesData[2] = {fatBinary1.data(),
                                     fatBinary2.data()};
    const uint64_t sourcesLen[2] = {fatBinary1.size(),
                                    fatBinary2.size()};
    const char *sourcesName[2] = {"fatBinary1.ar",
                                  "fatBinary2.ar"};

    uint32_t numOutputs;
    uint8_t **outputData;
    uint64_t *outputLen;
    char **outputName;
    const char *argv[] = {
        "ocloc",
        "concat",
        "fatBinary1.ar",
        "fatBinary2.ar",
        "-out",
        "catFatBinary.ar"};
    unsigned int argc = sizeof(argv) / sizeof(argv[0]);
    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             2, sourcesData, sourcesLen, sourcesName,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &outputData, &outputLen, &outputName);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(NEO::OclocErrorCode::SUCCESS, retVal);
    EXPECT_TRUE(output.empty());

    uint32_t fatBinaryIdx = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < numOutputs; i++) {
        if (strcmp(argv[argc - 1], outputName[i]) == 0) {
            fatBinaryIdx = i;
            break;
        }
    }
    ASSERT_NE(std::numeric_limits<uint32_t>::max(), fatBinaryIdx);

    std::string errors, warnings;
    auto ar = NEO::Ar::decodeAr(ArrayRef<const uint8_t>::fromAny(outputData[fatBinaryIdx], static_cast<size_t>(outputLen[fatBinaryIdx])),
                                errors, warnings);
    EXPECT_TRUE(errors.empty());
    EXPECT_TRUE(warnings.empty());

    bool hasFatBinary1 = false;
    bool hasFatBinary2 = false;
    for (auto &file : ar.files) {
        if (file.fileName == file1Name) {
            hasFatBinary1 = true;
            ASSERT_EQ(file1.size(), file.fileData.size());
            EXPECT_EQ(0, memcmp(file1.data(), file.fileData.begin(), file1.size()));
        } else if (file.fileName == file2Name) {
            hasFatBinary2 = true;
            ASSERT_EQ(file2.size(), file.fileData.size());
            EXPECT_EQ(0, memcmp(file2.data(), file.fileData.begin(), file2.size()));
        }
    }

    EXPECT_TRUE(hasFatBinary1);
    EXPECT_TRUE(hasFatBinary2);
    oclocFreeOutput(&numOutputs, &outputData, &outputLen, &outputName);
}
