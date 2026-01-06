/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/decoder/helper.h"
#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_concat.h"
#include "shared/offline_compiler/source/ocloc_interface.h"
#include "shared/offline_compiler/source/queries.h"
#include "shared/offline_compiler/source/utilities/get_git_version_info.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/mock_file_io.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_os_library.h"

#include "environment.h"
#include "gtest/gtest.h"
#include "hw_cmds_default.h"
#include "mock/mock_argument_helper.h"
#include "neo_aot_platforms.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>

extern Environment *gEnvironment;

using namespace std::string_literals;

Ocloc::SupportedDevicesHelper::SupportedDevicesData createMockFormerData() {
    Ocloc::SupportedDevicesHelper::SupportedDevicesData mockFormerData{
        // deviceIpVersions
        {
            0x2000000, 0x2400009, 0x2404009, 0x2408009, 0x240c000, 0x2410000, 0x2414000, 0x2418000, 0x241c000,
            0x2c00000, 0x2c04000, 0x2c08000},
        // deviceInfos
        {
            {0x1001, 0x01, 0x2000000},
            {0x1002, 0x02, 0x2400009},
            {0x1003, 0x03, 0x2404009},
            {0x1004, 0x04, 0x2408009}},
        // acronyms
        {
            {"bdw", 0x2000000},
            {"skl", 0x2400009},
            {"kbl", 0x2404009},
            {"cfl", 0x2408009},
            {"apl", 0x240c000},
            {"bxt", 0x240c000},
            {"glk", 0x2410000},
            {"whl", 0x2414000},
            {"aml", 0x2418000},
            {"cml", 0x241c000},
            {"icl", 0x2c00000},
            {"lkf", 0x2c04000},
            {"ehl", 0x2c08000},
        },
        // familyGroups
        {
            {"family8", {0x2000000}},
            {"family9", {0x2400009, 0x2404009, 0x2408009, 0x240c000, 0x2410000, 0x2414000, 0x2418000, 0x241c000}},
            {"family11", {0x2c00000, 0x2c04000, 0x2c08000}}},
        // releaseGroups
        {
            {"release8", {0x2000000}},
            {"release9", {0x2400009, 0x2404009, 0x2408009, 0x240c000, 0x2410000, 0x2414000, 0x2418000, 0x241c000}},
            {"release11", {0x2c00000, 0x2c04000, 0x2c08000}}}};

    return mockFormerData;
}

void mockedAbortOclocExecution(int errorCode) {
    throw std::runtime_error{"mockedAbortOclocExecution() called with error code = " + std::to_string(errorCode)};
}

namespace OclocTestMocks {
int mockOclocInvokeResult = ocloc_error_t::OCLOC_SUCCESS;
}

int mockOclocInvoke(unsigned int numArgs, const char *argv[],
                    const uint32_t numSources, const uint8_t **dataSources, const uint64_t *lenSources, const char **nameSources,
                    const uint32_t numInputHeaders, const uint8_t **dataInputHeaders, const uint64_t *lenInputHeaders, const char **nameInputHeaders,
                    uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs);

int mockOclocInvoke(unsigned int numArgs, const char *argv[],
                    const uint32_t numSources, const uint8_t **dataSources, const uint64_t *lenSources, const char **nameSources,
                    const uint32_t numInputHeaders, const uint8_t **dataInputHeaders, const uint64_t *lenInputHeaders, const char **nameInputHeaders,
                    uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs) {

    if (numOutputs && dataOutputs && lenOutputs && nameOutputs) {
        numOutputs[0] = 2;
        dataOutputs[0] = new uint8_t *[2];
        dataOutputs[0][0] = new uint8_t[1];
        dataOutputs[0][0][0] = 0xa;
        dataOutputs[0][1] = new uint8_t[2];
        dataOutputs[0][1][0] = 0x1;
        dataOutputs[0][1][1] = 0x4;
        lenOutputs[0] = new uint64_t[2];
        lenOutputs[0][0] = 1;
        lenOutputs[0][1] = 2;
        nameOutputs[0] = new char *[2];
        constexpr char outputName0[] = "out0";
        constexpr char outputName1[] = "out1";
        nameOutputs[0][0] = new char[sizeof(outputName0)];
        nameOutputs[0][1] = new char[sizeof(outputName1)];
        memcpy_s(nameOutputs[0][0], sizeof(outputName0), outputName0, sizeof(outputName0));
        memcpy_s(nameOutputs[0][1], sizeof(outputName1), outputName1, sizeof(outputName1));
    }

    return OclocTestMocks::mockOclocInvokeResult;
}

TEST(OclocApiTests, WhenOclocVersionIsCalledThenCurrentOclocVersionIsReturned) {
    EXPECT_EQ(ocloc_version_t::OCLOC_VERSION_CURRENT, oclocVersion());
}

namespace Ocloc {
extern std::string oclocFormerLibName;
}

class OclocApiTest : public ::testing::Test {
  protected:
    void SetUp() override {
        std::string spvFile = std::string("copybuffer") + "_" + gEnvironment->devicePrefix + ".spv";
        std::string binFile = std::string("copybuffer") + "_" + gEnvironment->devicePrefix + ".bin";
        std::string dbgFile = std::string("copybuffer") + "_" + gEnvironment->devicePrefix + ".dbg";

        constexpr unsigned char mockByteArray[] = {0x01, 0x02, 0x03, 0x04};
        std::string_view byteArrayView(reinterpret_cast<const char *>(mockByteArray), sizeof(mockByteArray));

        writeDataToFile(spvFile.c_str(), byteArrayView);
        writeDataToFile(binFile.c_str(), byteArrayView);
        writeDataToFile(dbgFile.c_str(), byteArrayView);
        writeDataToFile(clCopybufferFilename.c_str(), kernelSources);
    }

    // Helper struct to reduce repetition of IoFunctions mocking
    struct IoFunctionsMockHelper {
        static inline std::vector<std::string> currentExpectedFiles{"copybuffer.cl"};
        static inline std::string currentKernelSource{"example_kernel(){}"};

        VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen;
        VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose;
        VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> mockFseek;
        VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> mockFtell;
        VariableBackup<decltype(NEO::IoFunctions::freadPtr)> mockFread;

        static FILE *mockFopenImpl(const char *filename, const char *mode) {
            std::filesystem::path filePath = filename;
            std::string fileNameWithExtension = filePath.filename().string();
            auto itr = std::find(currentExpectedFiles.begin(), currentExpectedFiles.end(), fileNameWithExtension);
            if (itr != currentExpectedFiles.end()) {
                return reinterpret_cast<FILE *>(0x40);
            }
            return NULL;
        }

        static int mockFcloseImpl(FILE *stream) { return 0; }

        static int mockFseekImpl(FILE *stream, long int offset, int origin) { return 0; }

        static long int mockFtellImpl(FILE *stream) {
            std::stringstream fileStream(currentKernelSource);
            fileStream.seekg(0, std::ios::end);
            return static_cast<long int>(fileStream.tellg());
        }

        static size_t mockFreadImpl(void *ptr, size_t size, size_t count, FILE *stream) {
            std::stringstream fileStream(currentKernelSource);
            size_t totalBytes = size * count;
            fileStream.read(static_cast<char *>(ptr), totalBytes);
            return static_cast<size_t>(fileStream.gcount() / size);
        }

        IoFunctionsMockHelper(const std::vector<std::string> &expectedFiles = {"copybuffer.cl"},
                              const std::string &kernelSource = "example_kernel(){}") : mockFopen(&NEO::IoFunctions::fopenPtr, mockFopenImpl),
                                                                                        mockFclose(&NEO::IoFunctions::fclosePtr, mockFcloseImpl),
                                                                                        mockFseek(&NEO::IoFunctions::fseekPtr, mockFseekImpl),
                                                                                        mockFtell(&NEO::IoFunctions::ftellPtr, mockFtellImpl),
                                                                                        mockFread(&NEO::IoFunctions::freadPtr, mockFreadImpl) {
            currentExpectedFiles = expectedFiles;
            currentKernelSource = kernelSource;
        }
    };

    const std::string clCopybufferFilename = "some_kernel.cl";
    const std::string_view kernelSources = "example_kernel(){}";
};

TEST_F(OclocApiTest, WhenGoodArgsAreGivenThenSuccessIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "copybuffer.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> mockFseek(&NEO::IoFunctions::fseekPtr, [](FILE *stream, long int offset, int origin) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> mockFtell(&NEO::IoFunctions::ftellPtr, [](FILE *stream) -> long int {
        std::string kernelSources = "example_kernel(){}";
        std::stringstream fileStream(kernelSources);
        fileStream.seekg(0, std::ios::end);
        return static_cast<long int>(fileStream.tellg());
    });
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> mockFread(&NEO::IoFunctions::freadPtr, [](void *ptr, size_t size, size_t count, FILE *stream) -> size_t {
        std::string kernelSources = "example_kernel(){}";
        std::stringstream fileStream(kernelSources);
        size_t totalBytes = size * count;
        fileStream.read(static_cast<char *>(ptr), totalBytes);
        return static_cast<size_t>(fileStream.gcount() / size);
    });

    std::string clFileName(clFiles + "copybuffer.cl");
    const char *argv[] = {
        "ocloc",
        "-file",
        clFileName.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_SUCCESS);
    EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file test_files/copybuffer.cl -device "s + argv[4]));
    EXPECT_NE(std::string::npos, output.find("Build succeeded.\n"));
}

TEST_F(OclocApiTest, GivenQuietModeAndValidArgumentsWhenRunningOclocThenSuccessIsReturnedAndBuildSucceededMessageIsNotPrinted) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "copybuffer.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> mockFseek(&NEO::IoFunctions::fseekPtr, [](FILE *stream, long int offset, int origin) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> mockFtell(&NEO::IoFunctions::ftellPtr, [](FILE *stream) -> long int {
        std::string kernelSources = "example_kernel(){}";
        std::stringstream fileStream(kernelSources);
        fileStream.seekg(0, std::ios::end);
        return static_cast<long int>(fileStream.tellg());
    });
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> mockFread(&NEO::IoFunctions::freadPtr, [](void *ptr, size_t size, size_t count, FILE *stream) -> size_t {
        std::string kernelSources = "example_kernel(){}";
        std::stringstream fileStream(kernelSources);
        size_t totalBytes = size * count;
        fileStream.read(static_cast<char *>(ptr), totalBytes);
        return static_cast<size_t>(fileStream.gcount() / size);
    });

    std::string clFileName(clFiles + "copybuffer.cl");
    const char *argv[] = {
        "ocloc",
        "-file",
        clFileName.c_str(),
        "-q",
        "-device",
        gEnvironment->devicePrefix.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(OCLOC_SUCCESS, retVal);
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
    EXPECT_EQ(retVal, OCLOC_SUCCESS);
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

TEST(OclocApiTests, GivenIgcRevisionQueryWhenQueryingThenIgcRevisionIsReturned) {
    uint32_t numOutputs;
    uint64_t *lenOutputs;
    uint8_t **dataOutputs;
    char **nameOutputs;
    const char *argv[] = {
        "ocloc",
        "query",
        NEO::Queries::queryIgcRevision.data()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
    EXPECT_EQ(retVal, OCLOC_SUCCESS);
    EXPECT_EQ(numOutputs, 2u);

    int queryOutputIndex = -1;
    for (uint32_t i = 0; i < numOutputs; ++i) {
        if (strcmp(NEO::Queries::queryIgcRevision.data(), nameOutputs[i]) == 0) {
            queryOutputIndex = i;
        }
    }
    ASSERT_NE(-1, queryOutputIndex);
    NEO::ConstStringRef queryOutput(reinterpret_cast<const char *>(dataOutputs[queryOutputIndex]),
                                    static_cast<size_t>(lenOutputs[queryOutputIndex]));
    EXPECT_STREQ("mockigcrevision", queryOutput.data());

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
    EXPECT_EQ(retVal, OCLOC_SUCCESS);
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

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_INVALID_COMMAND_LINE);
    EXPECT_STREQ("Error: Invalid command line. Expected ocloc query <argument>. See ocloc query --help\nCommand was: ocloc query\n", output.c_str());
}

TEST(OclocApiTests, GivenInvalidQueryWhenQueryingThenErrorIsReturned) {
    const char *argv[] = {
        "ocloc",
        "query",
        "unknown_query"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_INVALID_COMMAND_LINE);
    EXPECT_STREQ("Error: Invalid command line.\nUnknown argument unknown_query\nCommand was: ocloc query unknown_query\n", output.c_str());
}

TEST(OclocApiTests, givenNoAcronymWhenIdsCommandIsInvokeThenErrorIsReported) {
    const char *argv[] = {
        "ocloc",
        "ids"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_INVALID_COMMAND_LINE);
    EXPECT_STREQ("Error: Invalid command line. Expected ocloc ids <acronym>.\nCommand was: ocloc ids\n", output.c_str());
}

TEST(OclocApiTests, givenUnknownAcronymWhenIdsCommandIsInvokeThenErrorIsReported) {
    const char *argv[] = {
        "ocloc",
        "ids",
        "unk"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_INVALID_COMMAND_LINE);
    EXPECT_STREQ("Error: Invalid command line. Unknown acronym unk.\nCommand was: ocloc ids unk\n", output.c_str());
}

TEST_F(OclocApiTest, WhenGoodFamilyNameIsProvidedThenSuccessIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "copybuffer.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> mockFseek(&NEO::IoFunctions::fseekPtr, [](FILE *stream, long int offset, int origin) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> mockFtell(&NEO::IoFunctions::ftellPtr, [](FILE *stream) -> long int {
        std::string kernelSource = "example_kernel(){}";
        std::stringstream fileStream(kernelSource);
        fileStream.seekg(0, std::ios::end);
        return static_cast<long int>(fileStream.tellg());
    });
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> mockFread(&NEO::IoFunctions::freadPtr, [](void *ptr, size_t size, size_t count, FILE *stream) -> size_t {
        std::string kernelSource = "example_kernel(){}";
        std::stringstream fileStream(kernelSource);
        size_t totalBytes = size * count;
        fileStream.read(static_cast<char *>(ptr), totalBytes);
        return static_cast<size_t>(fileStream.gcount() / size);
    });
    std::string clFileName(clFiles + "copybuffer.cl");
    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto allSupportedDeviceConfigs = argHelper->productConfigHelper->getDeviceAotInfo();
    if (allSupportedDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    std::string family("");
    for (const auto &config : allSupportedDeviceConfigs) {
        if (config.hwInfo->platform.eProductFamily == NEO::DEFAULT_PLATFORM::hwInfo.platform.eProductFamily) {
            family = ProductConfigHelper::getAcronymFromAFamily(config.family).str();
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

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);

    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_SUCCESS);
    EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file " + clFileName + " -device " + family));
}

TEST(OclocApiTests, WhenArgsWithMissingFileAreGivenThenErrorMessageIsProduced) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

    const char *argv[] = {
        "ocloc",
        "-q",
        "-file",
        "test_files/IDoNotExist.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_INVALID_FILE);
    EXPECT_NE(std::string::npos, output.find("Command was: ocloc -q -file test_files/IDoNotExist.cl -device "s + argv[5]));
}

TEST(OclocApiTests, givenInputOptionsAndInternalOptionsWhenCmdlineIsPrintedThenBothAreInQuotes) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

    const char *argv[] = {
        "ocloc",
        "-q",
        "-file",
        "test_files/IDoNotExist.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-options", "-D DEBUG -cl-kernel-arg-info", "-internal_options", "-internalOption1 -internal-option-2"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();
    EXPECT_NE(retVal, OCLOC_SUCCESS);
    EXPECT_TRUE(output.find("Command was: ocloc -q -file test_files/IDoNotExist.cl -device " +
                            gEnvironment->devicePrefix +
                            " -options \"-D DEBUG -cl-kernel-arg-info\" -internal_options \"-internalOption1 -internal-option-2\"") != std::string::npos);

    size_t quotesCount = std::count(output.begin(), output.end(), '\"');
    EXPECT_EQ(quotesCount, 4u);
}

TEST(OclocApiTests, givenInputOptionsCalledOptionsWhenCmdlineIsPrintedThenQuotesAreCorrect) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

    const char *argv[] = {
        "ocloc",
        "-q",
        "-file",
        "test_files/IDoNotExist.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-options", "-options", "-internal_options", "-internalOption"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();
    EXPECT_NE(retVal, OCLOC_SUCCESS);
    EXPECT_TRUE(output.find("Command was: ocloc -q -file test_files/IDoNotExist.cl -device " +
                            gEnvironment->devicePrefix +
                            " -options \"-options\" -internal_options \"-internalOption\"") != std::string::npos);

    size_t quotesCount = std::count(output.begin(), output.end(), '\"');
    EXPECT_EQ(quotesCount, 4u);
}

TEST_F(OclocApiTest, GivenIncludeHeadersWhenCompilingThenPassesToFclHeadersPackedAsElf) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

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

    oclocFreeOutput(&numOutputs, &outputs, &outputsLen, &ouputsNames);
}

TEST(OclocApiTests, GivenHelpParameterWhenDecodingThenHelpMsgIsPrintedAndSuccessIsReturned) {
    const char *argv[] = {
        "ocloc",
        "disasm",
        "--help"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();
    EXPECT_FALSE(output.empty());
    EXPECT_EQ(retVal, OCLOC_SUCCESS);
}

TEST(OclocApiTests, GivenNonExistingFileWhenDecodingThenAbortIsCalled) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });
    VariableBackup oclocAbortBackup{&abortOclocExecution, &mockedAbortOclocExecution};

    const char *argv[] = {
        "ocloc",
        "disasm",
        "-file",
        "nonexistent_file_that_should_fail",
        "-dump",
        "test_files/created"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    const auto retVal = oclocInvoke(argc, argv,
                                    0, nullptr, nullptr, nullptr,
                                    0, nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr, nullptr);
    const auto output = capture.getCapturedStdout();

    EXPECT_NE(std::string::npos, output.find("mockedAbortOclocExecution() called with error code = 1"));
    EXPECT_EQ(-1, retVal);
}

TEST(OclocApiTests, GivenMissingFileNameWhenDecodingThenErrorIsReturned) {
    const char *argv[] = {
        "ocloc",
        "disasm",
        "-file"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    const auto retVal = oclocInvoke(argc, argv,
                                    0, nullptr, nullptr, nullptr,
                                    0, nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr, nullptr);
    const auto output = capture.getCapturedStdout();

    EXPECT_NE(std::string::npos, output.find("Unknown argument -file\n"));
    EXPECT_EQ(-1, retVal);
}

TEST_F(OclocApiTest, GivenOnlySpirVWithMultipleDevicesWhenCompilingThenFirstDeviceIsSelected) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "copybuffer.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> mockFseek(&NEO::IoFunctions::fseekPtr, [](FILE *stream, long int offset, int origin) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> mockFtell(&NEO::IoFunctions::ftellPtr, [](FILE *stream) -> long int {
        std::string kernelSource = "example_kernel(){}";
        std::stringstream fileStream(kernelSource);
        fileStream.seekg(0, std::ios::end);
        return static_cast<long int>(fileStream.tellg());
    });
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> mockFread(&NEO::IoFunctions::freadPtr, [](void *ptr, size_t size, size_t count, FILE *stream) -> size_t {
        std::string kernelSource = "example_kernel(){}";
        std::stringstream fileStream(kernelSource);
        size_t totalBytes = size * count;
        fileStream.read(static_cast<char *>(ptr), totalBytes);
        return static_cast<size_t>(fileStream.gcount() / size);
    });

    std::string clFileName(clFiles + "copybuffer.cl");
    AOT::FAMILY productFamily = AOT::UNKNOWN_FAMILY;
    std::string familyAcronym("");
    std::string firstDeviceAcronym("");

    std::unique_ptr<OclocArgHelper> argHelper = std::make_unique<OclocArgHelper>();
    auto supportedDeviceConfigs = argHelper->productConfigHelper->getDeviceAotInfo();
    if (supportedDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    for (const auto &deviceConfig : supportedDeviceConfigs) {
        if (deviceConfig.hwInfo->platform.eProductFamily == NEO::DEFAULT_PLATFORM::hwInfo.platform.eProductFamily) {
            productFamily = deviceConfig.family;
            familyAcronym = ProductConfigHelper::getAcronymFromAFamily(deviceConfig.family).str();
            break;
        }
    }

    if (familyAcronym.empty()) {
        GTEST_SKIP();
    }

    const char *argv[] = {
        "ocloc",
        "-file",
        clFileName.c_str(),
        "-device",
        familyAcronym.c_str(),
        "-spv_only"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    for (const auto &deviceConfig : supportedDeviceConfigs) {
        if (deviceConfig.family == productFamily) {
            if (!deviceConfig.deviceAcronyms.empty()) {
                firstDeviceAcronym = deviceConfig.deviceAcronyms.front().str();
            } else if (!deviceConfig.rtlIdAcronyms.empty()) {
                firstDeviceAcronym = deviceConfig.rtlIdAcronyms.front().str();
            }
            break;
        }
    }

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_SUCCESS);
    EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -device " + firstDeviceAcronym + " -spv_only"));
}

TEST(OclocApiTests, GivenHelpParameterWhenCompilingThenHelpMsgIsPrintedAndSuccessIsReturned) {
    const char *argv[] = {
        "ocloc",
        "compile",
        "--help"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);

    std::string output = capture.getCapturedStdout();
    EXPECT_FALSE(output.empty());
    EXPECT_EQ(retVal, OCLOC_SUCCESS);
}

TEST(OclocApiTests, GivenHelpParameterWhenEncodingThenHelpMsgIsPrintedAndSuccessIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

    const char *argv[] = {
        "ocloc",
        "asm",
        "--help"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();
    EXPECT_FALSE(output.empty());
    EXPECT_EQ(retVal, OCLOC_SUCCESS);
}

TEST(OclocApiTests, GivenMissingDumpFileNameWhenEncodingThenErrorIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

    const char *argv[] = {
        "ocloc",
        "asm",
        "-dump"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    const auto retVal = oclocInvoke(argc, argv,
                                    0, nullptr, nullptr, nullptr,
                                    0, nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr, nullptr);
    const auto output = capture.getCapturedStdout();

    EXPECT_NE(std::string::npos, output.find("Unknown argument -dump\n"));
    EXPECT_EQ(-1, retVal);
}

TEST(OclocApiTests, GivenValidArgumentsAndMissingPtmFileWhenEncodingThenErrorIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

    const char *argv[] = {
        "ocloc",
        "asm",
        "-dump",
        "test_files/dump",
        "-out",
        "test_files/binary_gen.bin"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    const auto retVal = oclocInvoke(argc, argv,
                                    0, nullptr, nullptr, nullptr,
                                    0, nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr, nullptr);
    const auto output = capture.getCapturedStdout();

    EXPECT_NE(std::string::npos, output.find("Error! Couldn't find PTM.txt"));
    EXPECT_EQ(-1, retVal);
}

TEST(OclocApiTests, GiveMultiCommandHelpArgumentsWhenInvokingOclocThenHelpIsPrinted) {
    const char *argv[] = {
        "ocloc",
        "multi",
        "--help"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    const auto retVal = oclocInvoke(argc, argv,
                                    0, nullptr, nullptr, nullptr,
                                    0, nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr, nullptr);
    const auto output = capture.getCapturedStdout();
    EXPECT_FALSE(output.empty());

    EXPECT_EQ(-1, retVal);
}

TEST(OclocApiTests, GivenNonexistentFileWhenValidateIsInvokedThenErrorIsPrinted) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

    const char *argv[] = {
        "ocloc",
        "validate",
        "-file",
        "some_special_nonexistent_file.gen"};
    unsigned int argc = sizeof(argv) / sizeof(argv[0]);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);

    const auto output = capture.getCapturedStdout();
    EXPECT_EQ(-1, retVal);

    const std::string expectedErrorMessage{"Error : Input file missing : some_special_nonexistent_file.gen\nCommand was: ocloc validate -file some_special_nonexistent_file.gen\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST(OclocApiTests, GivenCommandWithoutArgsWhenOclocIsInvokedThenHelpIsPrinted) {
    const char *argv[] = {
        "ocloc"};
    unsigned int argc = sizeof(argv) / sizeof(argv[0]);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);

    const auto output = capture.getCapturedStdout();
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_FALSE(output.empty());
}

TEST(OclocApiTests, GivenHelpArgumentWhenOclocIsInvokedThenHelpIsPrinted) {
    static constexpr std::array flagsToTest = {"-h", "--help"};
    for (const auto helpFlag : flagsToTest) {
        const char *argv[] = {
            "ocloc",
            helpFlag};

        unsigned int argc = sizeof(argv) / sizeof(argv[0]);

        StreamCapture capture;
        capture.captureStdout();
        int retVal = oclocInvoke(argc, argv,
                                 0, nullptr, nullptr, nullptr,
                                 0, nullptr, nullptr, nullptr,
                                 nullptr, nullptr, nullptr, nullptr);

        const auto output = capture.getCapturedStdout();
        EXPECT_EQ(OCLOC_SUCCESS, retVal);
        EXPECT_FALSE(output.empty());
    }
}

TEST(OclocApiTests, GivenHelpParameterWhenLinkingThenHelpMsgIsPrintedAndSuccessIsReturned) {
    const char *argv[] = {
        "ocloc",
        "link",
        "--help"};
    unsigned int argc = sizeof(argv) / sizeof(argv[0]);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();
    EXPECT_FALSE(output.empty());
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
}

TEST(OclocApiTests, GivenInvalidParameterWhenLinkingThenErrorIsReturned) {
    const char *argv[] = {
        "ocloc",
        "link",
        "--dummy_param"};
    unsigned int argc = sizeof(argv) / sizeof(argv[0]);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();
    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);

    const std::string expectedInitError{"Invalid option (arg 2): --dummy_param\n"};
    const std::string expectedExecuteError{"Error: Linker cannot be executed due to unsuccessful initialization!\nCommand was: ocloc link --dummy_param\n"};
    const std::string expectedErrorMessage = expectedInitError + expectedExecuteError;
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST(OclocApiTests, GivenInvalidCommandLineWhenConcatenatingThenErrorIsReturned) {
    const char *argv[] = {
        "ocloc",
        "concat"};
    unsigned int argc = sizeof(argv) / sizeof(argv[0]);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();
    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);
    const std::string emptyCommandLineError = "No files to concatenate were provided.\n";
    const std::string expectedErrorMessage = emptyCommandLineError + NEO::OclocConcat::helpMessage.str() + "Command was: ocloc concat\n";
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

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             2, sourcesData, sourcesLen, sourcesName,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &outputData, &outputLen, &outputName);
    std::string output = capture.getCapturedStdout();
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
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

TEST_F(OclocApiTest, GivenVerboseModeWhenCompilingThenPrintCommandLine) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "copybuffer.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> mockFseek(&NEO::IoFunctions::fseekPtr, [](FILE *stream, long int offset, int origin) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> mockFtell(&NEO::IoFunctions::ftellPtr, [](FILE *stream) -> long int {
        std::string kernelSource = "example_kernel(){}";
        std::stringstream fileStream(kernelSource);
        fileStream.seekg(0, std::ios::end);
        return static_cast<long int>(fileStream.tellg());
    });
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> mockFread(&NEO::IoFunctions::freadPtr, [](void *ptr, size_t size, size_t count, FILE *stream) -> size_t {
        std::string kernelSource = "example_kernel(){}";
        std::stringstream fileStream(kernelSource);
        size_t totalBytes = size * count;
        fileStream.read(static_cast<char *>(ptr), totalBytes);
        return static_cast<size_t>(fileStream.gcount() / size);
    });

    std::string clFileName(clFiles + "copybuffer.cl");
    const char *argv[] = {
        "ocloc",
        "-file",
        clFileName.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-v"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_SUCCESS);
    EXPECT_NE(std::string::npos, output.find("Command was: ocloc -file "s + clFileName.c_str() + " -device "s + argv[4] + " -v")) << output;
    EXPECT_NE(std::string::npos, output.find("Build succeeded.\n"));
}

TEST_F(OclocApiTest, GivenFormerDeviceNamesWhenCompilingThenFormerOclocIsUsedAndSuccessIsReturned) {
    IoFunctionsMockHelper mockIoFunctions;

    std::string clFileName(clFiles + "copybuffer.cl");
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{{clFileName, "example_kernel(){}"}};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto formerHelper = std::make_unique<FormerProductConfigHelper>();

    formerHelper->getData() = createMockFormerData();
    mockArgHelper.setFormerProductConfigHelper(std::move(formerHelper));

    VariableBackup<std::string> oclocFormerNameBackup{&Ocloc::oclocFormerLibName};
    Ocloc::oclocFormerLibName = "oclocFormer";

    static auto storedOriginalLoadFunc = NEO::OsLibrary::loadFunc;

    auto selectiveLoadFunc = [](const NEO::OsLibraryCreateProperties &properties) -> NEO::OsLibrary * {
        if (properties.libraryName == "oclocFormer") {
            if (MockOsLibrary::loadLibraryNewObject) {
                auto ptr = MockOsLibrary::loadLibraryNewObject;
                MockOsLibrary::loadLibraryNewObject = nullptr;
                return ptr;
            }
            return nullptr;
        }
        return storedOriginalLoadFunc(properties);
    };
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, +selectiveLoadFunc};

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["oclocInvoke"] = reinterpret_cast<void *>(mockOclocInvoke);
    VariableBackup<int> retCodeBackup{&OclocTestMocks::mockOclocInvokeResult, ocloc_error_t::OCLOC_SUCCESS};

    std::vector<std::string> deviceCombinations = {
        "8.0.0",
        "0x1001",
        "33554432",
        "bdw",
        "bdw,skl,icl",
        "bdw:icl",
        ":icl"};

    for (const auto &deviceArg : deviceCombinations) {
        const char *argv[] = {
            "ocloc",
            "-file",
            clFileName.c_str(),
            "-device",
            deviceArg.c_str()};
        unsigned int argc = sizeof(argv) / sizeof(const char *);

        StreamCapture capture;
        capture.captureStdout();
        auto retVal = oclocInvokeWithHelper(&mockArgHelper, argc, argv);
        std::string output = capture.getCapturedStdout();

        EXPECT_EQ(retVal, OCLOC_SUCCESS) << "Failed for device: " << deviceArg;
        EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file test_files/copybuffer.cl -device " + deviceArg)) << "Unexpected command output for device: " << deviceArg;
    }
}

TEST_F(OclocApiTest, GivenMixedFormerAndCurrentDeviceNamesWhenCompilingThenCorrectOclocIsUsedAndSuccessIsReturned) {
    IoFunctionsMockHelper mockIoFunctions;

    std::string clFileName(clFiles + "copybuffer.cl");
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{{clFileName, "example_kernel(){}"}};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto formerHelper = std::make_unique<FormerProductConfigHelper>();

    formerHelper->getData() = createMockFormerData();
    mockArgHelper.setFormerProductConfigHelper(std::move(formerHelper));

    auto target = gEnvironment->devicePrefix;
    auto product = mockArgHelper.productConfigHelper->getProductConfigFromAcronym(target);
    if (product == AOT::UNKNOWN_ISA) {
        GTEST_SKIP();
    }

    VariableBackup<std::string> oclocFormerNameBackup{&Ocloc::oclocFormerLibName};
    Ocloc::oclocFormerLibName = "oclocFormer";

    static auto storedOriginalLoadFunc = NEO::OsLibrary::loadFunc;

    auto selectiveLoadFunc = [](const NEO::OsLibraryCreateProperties &properties) -> NEO::OsLibrary * {
        if (properties.libraryName == "oclocFormer") {
            if (MockOsLibrary::loadLibraryNewObject) {
                auto ptr = MockOsLibrary::loadLibraryNewObject;
                MockOsLibrary::loadLibraryNewObject = nullptr;
                return ptr;
            }
            return nullptr;
        }
        return storedOriginalLoadFunc(properties);
    };
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, +selectiveLoadFunc};

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["oclocInvoke"] = reinterpret_cast<void *>(mockOclocInvoke);
    VariableBackup<int> retCodeBackup{&OclocTestMocks::mockOclocInvokeResult, ocloc_error_t::OCLOC_SUCCESS};

    std::vector<std::string> deviceCombinations = {
        "bdw,skl," + std::string(target.c_str()),
        "bdw:" + std::string(target.c_str()),
        ":" + std::string(target.c_str())};

    for (const auto &deviceArg : deviceCombinations) {
        const char *argv[] = {
            "ocloc",
            "-file",
            clFileName.c_str(),
            "-device",
            deviceArg.c_str()};
        unsigned int argc = sizeof(argv) / sizeof(const char *);

        StreamCapture capture;
        capture.captureStdout();
        auto retVal = oclocInvokeWithHelper(&mockArgHelper, argc, argv);
        std::string output = capture.getCapturedStdout();

        EXPECT_EQ(retVal, OCLOC_SUCCESS) << "Failed for device: " << deviceArg;
        EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file test_files/copybuffer.cl -device " + deviceArg)) << "Unexpected command output for device: " << deviceArg;
    }
}

TEST_F(OclocApiTest, GivenFormerFamilyNamesWhenCompilingThenFormerOclocIsUsedAndSuccessIsReturned) {
    IoFunctionsMockHelper mockIoFunctions;

    std::string clFileName(clFiles + "copybuffer.cl");
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{{clFileName, "example_kernel(){}"}};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto formerHelper = std::make_unique<FormerProductConfigHelper>();

    formerHelper->getData() = createMockFormerData();
    mockArgHelper.setFormerProductConfigHelper(std::move(formerHelper));

    VariableBackup<std::string> oclocFormerNameBackup{&Ocloc::oclocFormerLibName};
    Ocloc::oclocFormerLibName = "oclocFormer";

    static auto storedOriginalLoadFunc = NEO::OsLibrary::loadFunc;

    auto selectiveLoadFunc = [](const NEO::OsLibraryCreateProperties &properties) -> NEO::OsLibrary * {
        if (properties.libraryName == "oclocFormer") {
            if (MockOsLibrary::loadLibraryNewObject) {
                auto ptr = MockOsLibrary::loadLibraryNewObject;
                MockOsLibrary::loadLibraryNewObject = nullptr;
                return ptr;
            }
            return nullptr;
        }
        return storedOriginalLoadFunc(properties);
    };
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, +selectiveLoadFunc};

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["oclocInvoke"] = reinterpret_cast<void *>(mockOclocInvoke);
    VariableBackup<int> retCodeBackup{&OclocTestMocks::mockOclocInvokeResult, ocloc_error_t::OCLOC_SUCCESS};

    std::vector<std::string> familyCombinations = {
        "family8",
        "family8,family9,family11",
        "family8:family11",
        ":family11"};

    for (const auto &familyArg : familyCombinations) {
        const char *argv[] = {
            "ocloc",
            "-file",
            clFileName.c_str(),
            "-device",
            familyArg.c_str()};
        unsigned int argc = sizeof(argv) / sizeof(const char *);

        StreamCapture capture;
        capture.captureStdout();
        auto retVal = oclocInvokeWithHelper(&mockArgHelper, argc, argv);
        std::string output = capture.getCapturedStdout();

        EXPECT_EQ(retVal, OCLOC_SUCCESS) << "Failed for device: " << familyArg;
        EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file test_files/copybuffer.cl -device " + familyArg)) << "Unexpected command output for device: " << familyArg;
    }
}

TEST_F(OclocApiTest, GivenMixedFormerAndCurrentFamilyNamesWhenCompilingThenCorrectOclocIsUsedAndSuccessIsReturned) {
    IoFunctionsMockHelper mockIoFunctions;

    std::string clFileName(clFiles + "copybuffer.cl");
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{{clFileName, "example_kernel(){}"}};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto formerHelper = std::make_unique<FormerProductConfigHelper>();

    formerHelper->getData() = createMockFormerData();
    mockArgHelper.setFormerProductConfigHelper(std::move(formerHelper));

    auto target = gEnvironment->devicePrefix;
    auto product = mockArgHelper.productConfigHelper->getProductConfigFromAcronym(target);
    if (product == AOT::UNKNOWN_ISA) {
        GTEST_SKIP();
    }

    auto &deviceAotInfo = mockArgHelper.productConfigHelper->getDeviceAotInfo();
    auto it = std::find_if(deviceAotInfo.begin(), deviceAotInfo.end(), ProductConfigHelper::findProductConfig(product));
    auto familyAcronym = mockArgHelper.productConfigHelper->getAcronymFromAFamily(it->family);

    VariableBackup<std::string> oclocFormerNameBackup{&Ocloc::oclocFormerLibName};
    Ocloc::oclocFormerLibName = "oclocFormer";

    static auto storedOriginalLoadFunc = NEO::OsLibrary::loadFunc;

    auto selectiveLoadFunc = [](const NEO::OsLibraryCreateProperties &properties) -> NEO::OsLibrary * {
        if (properties.libraryName == "oclocFormer") {
            if (MockOsLibrary::loadLibraryNewObject) {
                auto ptr = MockOsLibrary::loadLibraryNewObject;
                MockOsLibrary::loadLibraryNewObject = nullptr;
                return ptr;
            }
            return nullptr;
        }
        return storedOriginalLoadFunc(properties);
    };
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, +selectiveLoadFunc};

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["oclocInvoke"] = reinterpret_cast<void *>(mockOclocInvoke);
    VariableBackup<int> retCodeBackup{&OclocTestMocks::mockOclocInvokeResult, ocloc_error_t::OCLOC_SUCCESS};

    std::vector<std::string> familyCombinations = {
        "family8,family11," + std::string(familyAcronym.str().c_str()),
        "family8:" + std::string(familyAcronym.str().c_str()),
        ":" + std::string(familyAcronym.str().c_str())};

    for (const auto &familyArg : familyCombinations) {
        const char *argv[] = {
            "ocloc",
            "-file",
            clFileName.c_str(),
            "-device",
            familyArg.c_str()};
        unsigned int argc = sizeof(argv) / sizeof(const char *);

        StreamCapture capture;
        capture.captureStdout();
        auto retVal = oclocInvokeWithHelper(&mockArgHelper, argc, argv);
        std::string output = capture.getCapturedStdout();

        EXPECT_EQ(retVal, OCLOC_SUCCESS) << "Failed for device: " << familyArg;
        EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file test_files/copybuffer.cl -device " + familyArg)) << "Unexpected command output for device: " << familyArg;
    }
}

TEST_F(OclocApiTest, GivenFormerReleaseNamesWhenCompilingThenFormerOclocIsUsedAndSuccessIsReturned) {
    IoFunctionsMockHelper mockIoFunctions;

    std::string clFileName(clFiles + "copybuffer.cl");
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{{clFileName, "example_kernel(){}"}};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto formerHelper = std::make_unique<FormerProductConfigHelper>();

    formerHelper->getData() = createMockFormerData();
    mockArgHelper.setFormerProductConfigHelper(std::move(formerHelper));

    VariableBackup<std::string> oclocFormerNameBackup{&Ocloc::oclocFormerLibName};
    Ocloc::oclocFormerLibName = "oclocFormer";

    static auto storedOriginalLoadFunc = NEO::OsLibrary::loadFunc;

    auto selectiveLoadFunc = [](const NEO::OsLibraryCreateProperties &properties) -> NEO::OsLibrary * {
        if (properties.libraryName == "oclocFormer") {
            if (MockOsLibrary::loadLibraryNewObject) {
                auto ptr = MockOsLibrary::loadLibraryNewObject;
                MockOsLibrary::loadLibraryNewObject = nullptr;
                return ptr;
            }
            return nullptr;
        }
        return storedOriginalLoadFunc(properties);
    };
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, +selectiveLoadFunc};

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["oclocInvoke"] = reinterpret_cast<void *>(mockOclocInvoke);
    VariableBackup<int> retCodeBackup{&OclocTestMocks::mockOclocInvokeResult, ocloc_error_t::OCLOC_SUCCESS};

    std::vector<std::string> releaseCombinations = {
        "release8",
        "release8,release9,release11",
        "release8:release11",
        ":release11"};

    for (const auto &releaseArg : releaseCombinations) {
        const char *argv[] = {
            "ocloc",
            "-file",
            clFileName.c_str(),
            "-device",
            releaseArg.c_str()};
        unsigned int argc = sizeof(argv) / sizeof(const char *);

        StreamCapture capture;
        capture.captureStdout();
        auto retVal = oclocInvokeWithHelper(&mockArgHelper, argc, argv);
        std::string output = capture.getCapturedStdout();

        EXPECT_EQ(retVal, OCLOC_SUCCESS) << "Failed for device: " << releaseArg;
        EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file test_files/copybuffer.cl -device " + releaseArg)) << "Unexpected command output for device: " << releaseArg;
    }
}

TEST_F(OclocApiTest, GivenMixedFormerAndCurrentReleaseNamesWhenCompilingThenCorrectOclocIsUsedAndSuccessIsReturned) {
    IoFunctionsMockHelper mockIoFunctions;

    std::string clFileName(clFiles + "copybuffer.cl");
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{{clFileName, "example_kernel(){}"}};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto formerHelper = std::make_unique<FormerProductConfigHelper>();

    formerHelper->getData() = createMockFormerData();
    mockArgHelper.setFormerProductConfigHelper(std::move(formerHelper));

    auto target = gEnvironment->devicePrefix;
    auto product = mockArgHelper.productConfigHelper->getProductConfigFromAcronym(target);
    if (product == AOT::UNKNOWN_ISA) {
        GTEST_SKIP();
    }

    auto &deviceAotInfo = mockArgHelper.productConfigHelper->getDeviceAotInfo();
    auto it = std::find_if(deviceAotInfo.begin(), deviceAotInfo.end(), ProductConfigHelper::findProductConfig(product));
    auto releaseAcronym = mockArgHelper.productConfigHelper->getAcronymFromARelease(it->release);

    VariableBackup<std::string> oclocFormerNameBackup{&Ocloc::oclocFormerLibName};
    Ocloc::oclocFormerLibName = "oclocFormer";

    static auto storedOriginalLoadFunc = NEO::OsLibrary::loadFunc;

    auto selectiveLoadFunc = [](const NEO::OsLibraryCreateProperties &properties) -> NEO::OsLibrary * {
        if (properties.libraryName == "oclocFormer") {
            if (MockOsLibrary::loadLibraryNewObject) {
                auto ptr = MockOsLibrary::loadLibraryNewObject;
                MockOsLibrary::loadLibraryNewObject = nullptr;
                return ptr;
            }
            return nullptr;
        }
        return storedOriginalLoadFunc(properties);
    };
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, +selectiveLoadFunc};

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["oclocInvoke"] = reinterpret_cast<void *>(mockOclocInvoke);
    VariableBackup<int> retCodeBackup{&OclocTestMocks::mockOclocInvokeResult, ocloc_error_t::OCLOC_SUCCESS};

    std::vector<std::string> releaseCombinations = {
        "release8,release11," + std::string(releaseAcronym.str().c_str()),
        "release8:" + std::string(releaseAcronym.str().c_str()),
        ":" + std::string(releaseAcronym.str().c_str())};

    for (const auto &releaseArg : releaseCombinations) {
        const char *argv[] = {
            "ocloc",
            "-file",
            clFileName.c_str(),
            "-device",
            releaseArg.c_str()};
        unsigned int argc = sizeof(argv) / sizeof(const char *);

        StreamCapture capture;
        capture.captureStdout();
        auto retVal = oclocInvokeWithHelper(&mockArgHelper, argc, argv);
        std::string output = capture.getCapturedStdout();

        EXPECT_EQ(retVal, OCLOC_SUCCESS) << "Failed for device: " << releaseArg;
        EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file test_files/copybuffer.cl -device " + releaseArg)) << "Unexpected command output for device: " << releaseArg;
    }
}

TEST(InvokeFormerOclocTest, givenEmptyOrInvalidFormerOclocNameWhenInvokeFormerOclocThenNulloptIsReturned) {
    const char *argv[] = {
        "ocloc",
        "-file",
        "kernel.cl",
        "-device",
        "invalid_device"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    auto retVal = Ocloc::Commands::invokeFormerOcloc("", argc, argv,
                                                     0, nullptr, nullptr, nullptr,
                                                     0, nullptr, nullptr, nullptr,
                                                     nullptr, nullptr, nullptr, nullptr);

    EXPECT_FALSE(retVal.has_value());

    retVal = Ocloc::Commands::invokeFormerOcloc("invalidName", argc, argv,
                                                0, nullptr, nullptr, nullptr,
                                                0, nullptr, nullptr, nullptr,
                                                nullptr, nullptr, nullptr, nullptr);

    EXPECT_FALSE(retVal.has_value());
}

struct OclocFallbackTests : ::testing::Test {

    int callOclocForInvalidDevice() {
        const char *argv[] = {
            "ocloc",
            "-file",
            "kernel.cl",
            "-device",
            "invalid_device"};
        unsigned int argc = sizeof(argv) / sizeof(const char *);
        if (passOutputs) {
            auto retVal = oclocInvoke(argc, argv,
                                      0, nullptr, nullptr, nullptr,
                                      0, nullptr, nullptr, nullptr,
                                      &numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
            return retVal;
        } else {

            StreamCapture capture;
            capture.captureStdout();
            capture.captureStderr();

            auto retVal = oclocInvoke(argc, argv,
                                      0, nullptr, nullptr, nullptr,
                                      0, nullptr, nullptr, nullptr,
                                      nullptr, nullptr, nullptr, nullptr);
            capturedStdout = capture.getCapturedStdout();
            capturedStderr = capture.getCapturedStderr();
            return retVal;
        }
    }

    void TearDown() override {
        Ocloc::oclocFormerLibName.clear();
        Ocloc::oclocFormerLibName.shrink_to_fit();
        if (passOutputs) {
            oclocFreeOutput(&numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
        }
    }
    bool passOutputs = false;
    uint32_t numOutputs{};
    uint8_t **dataOutputs{};
    uint64_t *lenOutputs{};
    char **nameOutputs{};
    std::string capturedStdout;
    std::string capturedStderr;
    VariableBackup<std::string> oclocFormerNameBackup{&Ocloc::oclocFormerLibName};
};

TEST_F(OclocFallbackTests, GivenNoFormerOclocNameWhenInvalidDeviceErrorIsReturnedThenDontFallback) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });

    Ocloc::oclocFormerLibName = "";

    auto retVal = callOclocForInvalidDevice();

    EXPECT_EQ(ocloc_error_t::OCLOC_INVALID_DEVICE, retVal);
    EXPECT_NE(std::string::npos, capturedStdout.find("Could not determine device target: invalid_device.\n"));
    EXPECT_NE(std::string::npos, capturedStdout.find("Error: Cannot get HW Info for device invalid_device.\n"));
    EXPECT_NE(std::string::npos, capturedStdout.find("Command was: ocloc -file kernel.cl -device invalid_device\n"));

    EXPECT_EQ(std::string::npos, capturedStdout.find("Invalid device error, trying to fallback to former ocloc"));
    EXPECT_EQ(std::string::npos, capturedStdout.find("Couldn't load former ocloc"));
    EXPECT_TRUE(capturedStderr.empty());
}
