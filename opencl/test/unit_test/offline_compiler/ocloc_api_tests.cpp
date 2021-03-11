/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/offline_compiler/source/utilities/get_git_version_info.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"

#include "environment.h"
#include "gtest/gtest.h"
#include "hw_cmds.h"

#include <string>

extern Environment *gEnvironment;

using namespace std::string_literals;

TEST(OclocApiTests, WhenOclocVersionIsCalledThenCurrentOclocVersionIsReturned) {
    EXPECT_EQ(ocloc_version_t::OCLOC_VERSION_CURRENT, oclocVersion());
}

TEST(OclocApiTests, WhenGoodArgsAreGivenThenSuccessIsReturned) {
    const char *argv[] = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(retVal, NEO::OfflineCompiler::ErrorCode::SUCCESS);
    EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file test_files/copybuffer.cl -device "s + argv[4]));
}

TEST(OclocApiTests, WhenValidQueryArgumentWithoutQueryInvokeIsGivenThenErrorMessageIsProduced) {
    const char *argv[] = {
        "ocloc",
        "compile",
        "NEO_REVISION"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);
    uint32_t numOutputs;
    uint64_t *lenOutputs;
    uint8_t **dataOutputs;
    char **nameOutputs;
    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OfflineCompiler::ErrorCode::INVALID_COMMAND_LINE);
    EXPECT_NE(std::string::npos, output.find("Invalid option (arg 2): NEO_REVISION"));
}

TEST(OclocApiTests, WhenValidQueryArgumentWithExtraArgumentIsGivenThenErrorMessageIsProduced) {
    const char *argv[] = {
        "ocloc",
        "query",
        "NEO_REVISION",
        "extra"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);
    uint32_t numOutputs;
    uint64_t *lenOutputs;
    uint8_t **dataOutputs;
    char **nameOutputs;
    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OfflineCompiler::ErrorCode::INVALID_COMMAND_LINE);
    EXPECT_NE(std::string::npos, output.find("Invalid option (arg 2): NEO_REVISION"));
}

TEST(OclocApiTests, WhenRightArgumentsToQueryOptionAreGivenThenSuccessIsReturned) {
    const char *argv[] = {
        "ocloc",
        "query",
        "NEO_REVISION"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);
    uint32_t numOutputs;
    uint64_t *lenOutputs;
    uint8_t **dataOutputs;
    char **nameOutputs;
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
    EXPECT_EQ(retVal, NEO::OfflineCompiler::ErrorCode::SUCCESS);
    EXPECT_EQ(numOutputs, 2u);
    EXPECT_STREQ(*nameOutputs, "NEO_REVISION");
    const char *revision = reinterpret_cast<const char *>(*dataOutputs);
    EXPECT_STREQ(revision, NEO::getRevision().c_str());

    oclocFreeOutput(&numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);

    const char *argv1[] = {
        "ocloc",
        "query",
        "OCL_DRIVER_VERSION"};

    retVal = oclocInvoke(argc, argv1,
                         0, nullptr, nullptr, nullptr,
                         0, nullptr, nullptr, nullptr,
                         &numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
    EXPECT_EQ(retVal, NEO::OfflineCompiler::ErrorCode::SUCCESS);
    EXPECT_EQ(numOutputs, 2u);
    EXPECT_STREQ(*nameOutputs, "OCL_DRIVER_VERSION");
    const char *driverVersion = reinterpret_cast<const char *>(*dataOutputs);
    EXPECT_STREQ(driverVersion, NEO::getOclDriverVersion().c_str());

    oclocFreeOutput(&numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
}

TEST(OclocApiTests, WhenQueryCommandPassedWithWrongArgumentThenErrorMessageIsProduced) {
    const char *argv[] = {
        "ocloc",
        "query",
        "wrong"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);
    uint32_t numOutputs;
    uint64_t *lenOutputs;
    uint8_t **dataOutputs;
    char **nameOutputs;
    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OfflineCompiler::ErrorCode::INVALID_COMMAND_LINE);
    EXPECT_NE(std::string::npos, output.find("Invalid option (arg 2): wrong"));
}

TEST(OclocApiTests, WhenQueryCommandPassedWithoutArgumentsThenErrorMessageIsProduced) {
    const char *argv[] = {
        "ocloc",
        "query"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);
    uint32_t numOutputs;
    uint64_t *lenOutputs;
    uint8_t **dataOutputs;
    char **nameOutputs;
    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(retVal, NEO::OfflineCompiler::ErrorCode::INVALID_COMMAND_LINE);
    EXPECT_NE(std::string::npos, output.find("Error: no options for query provided."));
}

TEST(OclocApiTests, WhenGoodFamilyNameIsProvidedThenSuccessIsReturned) {
    const char *argv[] = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        NEO::familyName[NEO::DEFAULT_PLATFORM::hwInfo.platform.eRenderCoreFamily]};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(retVal, NEO::OfflineCompiler::ErrorCode::SUCCESS);
    EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file test_files/copybuffer.cl -device "s + argv[4]));
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

    EXPECT_EQ(retVal, NEO::OfflineCompiler::ErrorCode::INVALID_FILE);
    EXPECT_NE(std::string::npos, output.find("Command was: ocloc -q -file test_files/IDoNotExist.cl -device "s + argv[5]));
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
