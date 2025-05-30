/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/decoder/translate_platform_base.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/mock_file_io.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"

#include "opencl/test/unit_test/offline_compiler/environment.h"
#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"
#include "opencl/test/unit_test/offline_compiler/stdout_capturer.h"
#include "opencl/test/unit_test/test_files/patch_list.h"

#include "gtest/gtest.h"
#include "igad.h"
#include "mock/mock_decoder.h"
#include "neo_igfxfmid.h"

#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

extern Environment *gEnvironment;

static void abortOclocExecutionMock(int code) {
    throw std::runtime_error{"Exit called with code = " + std::to_string(code)};
}

SProgramBinaryHeader createProgramBinaryHeader(const uint32_t numberOfKernels, const uint32_t patchListSize) {
    return SProgramBinaryHeader{MAGIC_CL, 0, 0, 0, numberOfKernels, 0, patchListSize};
}

SKernelBinaryHeaderCommon createKernelBinaryHeaderCommon(const uint32_t kernelNameSize, const uint32_t patchListSize) {
    SKernelBinaryHeaderCommon kernelHeader = {};
    kernelHeader.CheckSum = 0xFFFFFFFF;
    kernelHeader.ShaderHashCode = 0xFFFFFFFFFFFFFFFF;
    kernelHeader.KernelNameSize = kernelNameSize;
    kernelHeader.PatchListSize = patchListSize;
    return kernelHeader;
}

namespace NEO {
TEST(DecoderTests, GivenArgHelperWithHeadersWhenLoadingPatchListThenHeadersAreReturned) {
    const char input[] = "First\nSecond\nThird";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};

    MockDecoder decoder;
    decoder.mockArgHelper->headers.push_back(source);

    const auto lines = decoder.loadPatchList();
    ASSERT_EQ(3u, lines.size());

    EXPECT_EQ("First", lines[0]);
    EXPECT_EQ("Second", lines[1]);
    EXPECT_EQ("Third", lines[2]);
}

TEST(DecoderTests, GivenHeadersWithoutProgramBinaryHeaderWhenParsingTokensThenErrorIsRaised) {
    VariableBackup oclocAbortBackup{&abortOclocExecution, &abortOclocExecutionMock};

    const char input[] = "First\nSecond\nThird";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};

    MockDecoder decoder{false};
    decoder.mockArgHelper->headers.push_back(source);

    StdoutCapturer capturer{};
    EXPECT_ANY_THROW(decoder.parseTokens());
    const auto output{capturer.acquireOutput()};

    EXPECT_EQ("While parsing patchtoken definitions: couldn't find SProgramBinaryHeader.", output);
}

TEST(DecoderTests, GivenHeadersWithoutPatchTokenEnumWhenParsingTokensThenErrorIsRaised) {
    VariableBackup oclocAbortBackup{&abortOclocExecution, &abortOclocExecutionMock};

    const char input[] = "struct SProgramBinaryHeader\n{};";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};

    MockDecoder decoder{false};
    decoder.mockArgHelper->headers.push_back(source);

    StdoutCapturer capturer{};
    EXPECT_ANY_THROW(decoder.parseTokens());
    const auto output{capturer.acquireOutput()};

    EXPECT_EQ("While parsing patchtoken definitions: couldn't find enum PATCH_TOKEN.", output);
}

TEST(DecoderTests, GivenHeadersWithoutKernelBinaryHeaderWhenParsingTokensThenErrorIsRaised) {
    VariableBackup oclocAbortBackup{&abortOclocExecution, &abortOclocExecutionMock};

    const char input[] = "struct SProgramBinaryHeader\n{};\nenum PATCH_TOKEN\n{};";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};

    MockDecoder decoder{false};
    decoder.mockArgHelper->headers.push_back(source);

    StdoutCapturer capturer{};
    EXPECT_ANY_THROW(decoder.parseTokens());
    const auto output{capturer.acquireOutput()};

    EXPECT_EQ("While parsing patchtoken definitions: couldn't find SKernelBinaryHeader.", output);
}

TEST(DecoderTests, GivenHeadersWithoutKernelBinaryHeaderCommonWhenParsingTokensThenErrorIsRaised) {
    VariableBackup oclocAbortBackup{&abortOclocExecution, &abortOclocExecutionMock};

    const char input[] = "struct SProgramBinaryHeader\n{};\nenum PATCH_TOKEN\n{};struct SKernelBinaryHeader\n{};";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};

    MockDecoder decoder{false};
    decoder.mockArgHelper->headers.push_back(source);

    StdoutCapturer capturer{};
    EXPECT_ANY_THROW(decoder.parseTokens());
    const auto output{capturer.acquireOutput()};

    EXPECT_EQ("While parsing patchtoken definitions: couldn't find SKernelBinaryHeaderCommon.", output);
}

TEST(DecoderTests, GivenFieldsWithSizeWhenDumpingThemThenTheyAreWrittenToPtmFileStream) {
    VariableBackup oclocAbortBackup{&abortOclocExecution, &abortOclocExecutionMock};

    // Raw data contains 4 variables:
    // 1. UINT8_T = 0x01.
    // 2. UINT16_T = 0x0202
    // 3. UINT32_T = 0x03030303
    // 4. UINT64_T = 0x0404040404040404
    uint8_t rawData[] = {
        0x1,
        0x2, 0x2,
        0x3, 0x3, 0x3, 0x3,
        0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4};

    MockDecoder decoder;
    std::stringstream ptFileOutputStream;
    ptFileOutputStream << std::hex;

    const void *memoryPtr = rawData;
    for (uint8_t varSize = 1; varSize <= 8; varSize *= 2) {
        PTField field{varSize, "SomeUINT_" + std::to_string(varSize)};
        decoder.dumpField(memoryPtr, field, ptFileOutputStream);
    }

    const std::string expectedPtFileContent{
        "\t1 SomeUINT_1 1\n"
        "\t2 SomeUINT_2 202\n"
        "\t4 SomeUINT_4 3030303\n"
        "\t8 SomeUINT_8 404040404040404\n"};
    EXPECT_EQ(expectedPtFileContent, ptFileOutputStream.str());
}

TEST(DecoderTests, GivenInvalidFieldSizeWhenDumpingItThenErrorIsRaised) {
    VariableBackup oclocAbortBackup{&abortOclocExecution, &abortOclocExecutionMock};

    uint8_t rawData[] = {
        0x1,
        0x2, 0x2,
        0x3, 0x3, 0x3, 0x3};

    MockDecoder decoder{false};
    std::stringstream ptFileOutputStream;

    const void *memoryPtr = rawData;
    PTField badField{7, "SomeUINT_7"};

    StdoutCapturer capturer{};
    EXPECT_ANY_THROW(decoder.dumpField(memoryPtr, badField, ptFileOutputStream));
    const auto output{capturer.acquireOutput()};

    EXPECT_EQ("Error! Unknown size.\n", output);
}

TEST(DecoderTests, GivenPassingParsingAndNullptrDevBinaryWhenDecodingThenErrorIsRaised) {
    VariableBackup oclocAbortBackup{&abortOclocExecution, &abortOclocExecutionMock};

    MockDecoder decoder{false};
    decoder.callBaseParseTokens = false;
    decoder.callBaseGetDevBinary = false;
    decoder.devBinaryToReturn = nullptr;

    StdoutCapturer capturer{};
    EXPECT_ANY_THROW(decoder.decode());
    const auto output{capturer.acquireOutput()};

    EXPECT_EQ("Error! Device Binary section was not found.\n", output);
}

TEST(DecoderTests, GivenPassingParsingAndEmptyDevBinaryWhenDecodingThenWarningAboutZeroKernelIsPrinted) {
    VariableBackup oclocAbortBackup{&abortOclocExecution, &abortOclocExecutionMock};

    MockDecoder decoder{false};
    decoder.callBaseParseTokens = false;
    decoder.callBaseGetDevBinary = false;
    decoder.devBinaryToReturn = "";

    int decodeReturnValue{-1};

    StdoutCapturer capturer{};
    ASSERT_NO_THROW(decodeReturnValue = decoder.decode());
    const auto output{capturer.acquireOutput()};

    EXPECT_EQ(0, decodeReturnValue);
    EXPECT_EQ("Warning! Number of Kernels is 0.\n", output);
}

TEST(DecoderTests, GivenEmptyKernelHeaderWhenProcessingKernelThenErrorIsRaised) {
    VariableBackup oclocAbortBackup{&abortOclocExecution, &abortOclocExecutionMock};

    MockDecoder decoder{false};
    const void *memory = "abcdef";
    std::stringstream ptFile;

    StdoutCapturer capturer{};
    EXPECT_ANY_THROW(decoder.processKernel(memory, 0, ptFile));
    const auto output{capturer.acquireOutput()};

    EXPECT_EQ("Error! KernelNameSize was 0.\n", output);
}

TEST(DecoderTests, WhenParsingValidListOfParametersThenReturnValueIsZero) {
    const std::vector<std::string> args = {
        "ocloc",
        "disasm",
        "-file",
        "test_files/binary.bin",
        "-patch",
        "test_files/patch",
        "-dump",
        "test_files/created"};

    MockDecoder decoder;
    EXPECT_EQ(0, decoder.validateInput(args));
}

TEST(DecoderTests, GivenFlagsWhichRequireMoreArgsWithoutThemWhenParsingThenErrorIsReported) {
    const std::array<std::string, 4> flagsToTest = {
        "-file", "-device", "-patch", "-dump"};

    for (const auto &flag : flagsToTest) {
        const std::vector<std::string> args = {
            "ocloc",
            "disasm",
            flag};

        constexpr auto suppressMessages{false};
        MockDecoder decoder{suppressMessages};

        decoder.mockArgHelper->messagePrinter.setSuppressMessages(true);
        const auto result = decoder.validateInput(args);
        const auto output{decoder.mockArgHelper->getLog()};

        EXPECT_EQ(-1, result);

        const std::string expectedErrorMessage{"Unknown argument " + flag + "\n"};
        EXPECT_EQ(expectedErrorMessage, output);
    }
}

TEST(DecoderTests, givenUnknownDeviceNameWhenValidateInputThenCorrectWarningIsReported) {
    const std::vector<std::string> args = {
        "ocloc",
        "disasm",
        "-device",
        "unk"};

    constexpr auto suppressMessages{false};
    MockDecoder decoder{suppressMessages};

    decoder.mockArgHelper->messagePrinter.setSuppressMessages(true);
    const auto result = decoder.validateInput(args);
    const auto output{decoder.mockArgHelper->getLog()};
    EXPECT_EQ(result, 0);

    const std::string expectedWarningMessage{"Warning : missing or invalid -device parameter - results may be inaccurate\n"};
    EXPECT_TRUE(hasSubstr(output, expectedWarningMessage));
}

TEST(DecoderTests, givenDeprecatedDeviceNamesWhenValidateInputThenCorrectWarningIsReported) {
    constexpr auto suppressMessages{false};
    MockDecoder decoder{suppressMessages};

    auto deprecatedAcronyms = decoder.mockArgHelper->productConfigHelper->getDeprecatedAcronyms();

    for (const auto &acronym : deprecatedAcronyms) {
        const std::vector<std::string> args = {
            "ocloc",
            "disasm",
            "-device",
            acronym.str()};

        decoder.mockArgHelper->messagePrinter.setSuppressMessages(true);
        const auto result = decoder.validateInput(args);
        const auto output{decoder.mockArgHelper->getLog()};

        EXPECT_EQ(result, 0);

        const std::string expectedWarningMessage{"Warning : Deprecated device name is being used.\n"};
        EXPECT_TRUE(hasSubstr(output, expectedWarningMessage));
    }
}

TEST(DecoderTests, givenProductNamesThatExistsForIgaWhenValidateInputThenSuccessIsReturned) {
    constexpr auto suppressMessages{false};
    MockDecoder decoder{suppressMessages};
    if (!decoder.getMockIga()->isValidPlatform()) {
        GTEST_SKIP();
    }

    decoder.mockArgHelper->hasOutput = true;
    auto aotInfos = decoder.mockArgHelper->productConfigHelper->getDeviceAotInfo();

    for (const auto &device : aotInfos) {
        if (productFamily != device.hwInfo->platform.eProductFamily)
            continue;

        for (const auto *acronyms : {&device.deviceAcronyms, &device.rtlIdAcronyms}) {
            for (const auto &acronym : *acronyms) {
                const std::vector<std::string> args = {
                    "ocloc",
                    "disasm",
                    "-device",
                    acronym.str()};

                decoder.mockArgHelper->messagePrinter.setSuppressMessages(true);
                const auto result = decoder.validateInput(args);
                const auto output{decoder.mockArgHelper->getLog()};

                EXPECT_EQ(result, 0);
                EXPECT_TRUE(output.empty());
            }
        }
    }

    decoder.mockArgHelper->hasOutput = false;
}

TEST(DecoderTests, GivenIgnoreIsaPaddingFlagWhenParsingValidListOfParametersThenReturnValueIsZeroAndInternalFlagIsSet) {
    const std::vector<std::string> args = {
        "ocloc",
        "disasm",
        "-file",
        "test_files/binary.bin",
        "-patch",
        "test_files/patch",
        "-dump",
        "test_files/created",
        "-ignore_isa_padding"};

    MockDecoder decoder;
    EXPECT_EQ(0, decoder.validateInput(args));
    EXPECT_TRUE(decoder.ignoreIsaPadding);
}

TEST(DecoderTests, GivenQuietModeFlagWhenParsingValidListOfParametersThenReturnValueIsZeroAndMessagesAreSuppressed) {
    const std::vector<std::string> args = {
        "ocloc",
        "disasm",
        "-file",
        "test_files/binary.bin",
        "-patch",
        "test_files/patch",
        "-dump",
        "test_files/created",
        "-q"};

    constexpr auto suppressMessages{false};
    MockDecoder decoder{suppressMessages};

    EXPECT_EQ(0, decoder.validateInput(args));
    EXPECT_TRUE(decoder.argHelper->getPrinterRef().isSuppressed());
}

TEST(DecoderTests, GivenMissingDumpFlagWhenParsingValidListOfParametersThenReturnValueIsZeroAndWarningAboutCreationOfDefaultDirectoryIsPrinted) {
    constexpr auto suppressMessages{false};
    MockDecoder decoder{suppressMessages};

    if (gEnvironment->productConfig.empty() || !decoder.getMockIga()->isValidPlatform()) {
        GTEST_SKIP();
    }

    const std::vector<std::string> args = {
        "ocloc",
        "disasm",
        "-file",
        "test_files/binary.bin",
        "-device",
        gEnvironment->productConfig.c_str(),
        "-patch",
        "test_files/patch"};

    decoder.mockArgHelper->messagePrinter.setSuppressMessages(true);
    const auto result = decoder.validateInput(args);
    const auto output{decoder.mockArgHelper->getLog()};

    EXPECT_EQ(0, result);

    const std::string expectedErrorMessage{"Warning : Path to dump folder not specificed - using ./dump as default.\n"};
    EXPECT_TRUE(hasSubstr(output, expectedErrorMessage));
}

TEST(DecoderTests, GivenMissingDumpFlagAndArgHelperOutputEnabledWhenParsingValidListOfParametersThenReturnValueIsZeroAndDefaultDirectoryWarningIsNotEmitted) {
    constexpr auto suppressMessages{false};
    MockDecoder decoder{suppressMessages};

    if (gEnvironment->productConfig.empty() || !decoder.getMockIga()->isValidPlatform()) {
        GTEST_SKIP();
    }
    const std::vector<std::string> args = {
        "ocloc",
        "disasm",
        "-file",
        "test_files/binary.bin",
        "-device",
        gEnvironment->productConfig.c_str(),
        "-patch",
        "test_files/patch"};

    decoder.mockArgHelper->hasOutput = true;

    decoder.mockArgHelper->messagePrinter.setSuppressMessages(true);
    const auto result = decoder.validateInput(args);
    const auto output{decoder.mockArgHelper->getLog()};
    EXPECT_EQ(0, result);
    EXPECT_TRUE(output.empty()) << output;
    decoder.mockArgHelper->hasOutput = false;
}

TEST(DecoderTests, GivenValidSizeStringWhenGettingSizeThenProperOutcomeIsExpectedAndExceptionIsNotThrown) {
    MockDecoder decoder;
    EXPECT_EQ(static_cast<uint8_t>(1), decoder.getSize("uint8_t"));
    EXPECT_EQ(static_cast<uint8_t>(2), decoder.getSize("uint16_t"));
    EXPECT_EQ(static_cast<uint8_t>(4), decoder.getSize("uint32_t"));
    EXPECT_EQ(static_cast<uint8_t>(8), decoder.getSize("uint64_t"));
}

TEST(DecoderTests, GivenProperStructWhenReadingStructFieldsThenFieldsVectorGetsPopulatedCorrectly) {
    std::vector<std::string> lines;
    lines.push_back("/*           */");
    lines.push_back("struct SPatchSamplerStateArray :");
    lines.push_back("       SPatchItemHeader");
    lines.push_back("{");
    lines.push_back("    uint64_t   SomeField;");
    lines.push_back("    uint32_t   Offset;");
    lines.push_back("");
    lines.push_back("    uint16_t   Count;");
    lines.push_back("    uint8_t    BorderColorOffset;");
    lines.push_back("};");

    std::vector<PTField> fields;
    MockDecoder decoder;
    size_t pos = 4;
    uint32_t fullSize = decoder.readStructFields(lines, pos, fields);
    EXPECT_EQ(static_cast<uint32_t>(15), fullSize);

    EXPECT_EQ(static_cast<uint8_t>(8), fields[0].size);
    EXPECT_EQ("SomeField", fields[0].name);

    EXPECT_EQ(static_cast<uint8_t>(4), fields[1].size);
    EXPECT_EQ("Offset", fields[1].name);

    EXPECT_EQ(static_cast<uint8_t>(2), fields[2].size);
    EXPECT_EQ("Count", fields[2].name);

    EXPECT_EQ(static_cast<uint8_t>(1), fields[3].size);
    EXPECT_EQ("BorderColorOffset", fields[3].name);
}

TEST(DecoderTests, GivenProperPatchListFileWhenParsingTokensThenFileIsParsedCorrectly) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });

    MockDecoder decoder;
    decoder.parseTokens();

    EXPECT_EQ(static_cast<uint32_t>(28), (decoder.programHeader.size));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.programHeader.fields[0].size));
    EXPECT_EQ("Magic", (decoder.programHeader.fields[0].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.programHeader.fields[1].size));
    EXPECT_EQ("Version", (decoder.programHeader.fields[1].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.programHeader.fields[2].size));
    EXPECT_EQ("Device", (decoder.programHeader.fields[2].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.programHeader.fields[3].size));
    EXPECT_EQ("GPUPointerSizeInBytes", (decoder.programHeader.fields[3].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.programHeader.fields[4].size));
    EXPECT_EQ("NumberOfKernels", (decoder.programHeader.fields[4].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.programHeader.fields[5].size));
    EXPECT_EQ("SteppingId", (decoder.programHeader.fields[5].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.programHeader.fields[6].size));
    EXPECT_EQ("PatchListSize", (decoder.programHeader.fields[6].name));

    EXPECT_EQ(static_cast<uint8_t>(40), (decoder.kernelHeader.size));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.kernelHeader.fields[0].size));
    EXPECT_EQ("CheckSum", (decoder.kernelHeader.fields[0].name));
    EXPECT_EQ(static_cast<uint8_t>(8), (decoder.kernelHeader.fields[1].size));
    EXPECT_EQ("ShaderHashCode", (decoder.kernelHeader.fields[1].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.kernelHeader.fields[2].size));
    EXPECT_EQ("KernelNameSize", (decoder.kernelHeader.fields[2].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.kernelHeader.fields[3].size));
    EXPECT_EQ("PatchListSize", (decoder.kernelHeader.fields[3].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.kernelHeader.fields[4].size));
    EXPECT_EQ("KernelHeapSize", (decoder.kernelHeader.fields[4].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.kernelHeader.fields[5].size));
    EXPECT_EQ("GeneralStateHeapSize", (decoder.kernelHeader.fields[5].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.kernelHeader.fields[6].size));
    EXPECT_EQ("DynamicStateHeapSize", (decoder.kernelHeader.fields[6].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.kernelHeader.fields[7].size));
    EXPECT_EQ("SurfaceStateHeapSize", (decoder.kernelHeader.fields[7].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.kernelHeader.fields[8].size));
    EXPECT_EQ("KernelUnpaddedSize", (decoder.kernelHeader.fields[8].name));

    EXPECT_EQ(static_cast<uint8_t>(8), decoder.patchTokens[42]->size);
    EXPECT_EQ("PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO", decoder.patchTokens[42]->name);
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.patchTokens[42]->fields[0].size));
    EXPECT_EQ("ConstantBufferIndex", (decoder.patchTokens[42]->fields[0].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.patchTokens[42]->fields[1].size));
    EXPECT_EQ("InlineDataSize", (decoder.patchTokens[42]->fields[1].name));
}

TEST(DecoderTests, WhenPathToPatchTokensNotProvidedThenUseDefaults) {
    MockDecoder decoder;
    decoder.pathToPatch = "";
    decoder.parseTokens();
    EXPECT_NE(0U, decoder.programHeader.size);
    EXPECT_NE(0U, decoder.kernelHeader.size);
}

TEST(DecoderTests, GivenValidBinaryWhenReadingPatchTokensFromBinaryThenBinaryIsReadCorrectly) {
    std::string binaryString;
    std::stringstream binarySS;
    uint8_t byte;
    uint32_t byte4;

    byte4 = 4;
    binarySS.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 16;
    binarySS.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 1234;
    binarySS.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 5678;
    binarySS.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 2;
    binarySS.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 12;
    binarySS.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte = 255;
    for (auto i = 0; i < 4; ++i) {
        binarySS.write(reinterpret_cast<char *>(&byte), sizeof(uint8_t));
    }
    binaryString = binarySS.str();

    std::vector<char> binary(binaryString.begin(), binaryString.end());
    MockDecoder decoder;
    std::stringstream out;
    auto patchToken = std::make_unique<PatchToken>();
    patchToken->size = 20;
    patchToken->name = "Example patchtoken";
    patchToken->fields.push_back(PTField{4, "First"});
    patchToken->fields.push_back(PTField{4, "Second"});
    decoder.patchTokens.insert(std::pair<uint8_t, std::unique_ptr<PatchToken>>(4, std::move(patchToken)));
    const void *ptr = reinterpret_cast<void *>(binary.data());
    decoder.readPatchTokens(ptr, 28, out);
    std::string s = "Example patchtoken:\n\t4 Token 4\n\t4 Size 16\n\t4 First 1234\n\t4 Second 5678\nUnidentified PatchToken:\n\t4 Token 2\n\t4 Size 12\n\tHex ff ff ff ff\n";
    EXPECT_EQ(s, out.str());
}

TEST(DecoderTests, GivenValidBinaryWithoutPatchTokensWhenProcessingBinaryThenBinaryIsReadCorrectly) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });

    auto programHeader = createProgramBinaryHeader(1, 0);
    std::string kernelName("ExampleKernel");
    auto kernelHeader = createKernelBinaryHeaderCommon(static_cast<uint32_t>(kernelName.size() + 1), 0);

    std::stringstream binarySS;
    binarySS.write(reinterpret_cast<char *>(&programHeader), sizeof(SProgramBinaryHeader));
    binarySS.write(reinterpret_cast<char *>(&kernelHeader), sizeof(SKernelBinaryHeaderCommon));
    binarySS.write(kernelName.c_str(), kernelHeader.KernelNameSize);

    std::stringstream ptmFile;
    MockDecoder decoder;
    decoder.pathToDump = "non_existing_folder/";
    decoder.parseTokens();

    std::string binaryString = binarySS.str();
    std::vector<unsigned char> binary(binaryString.begin(), binaryString.end());
    const void *ptr = reinterpret_cast<void *>(binary.data());
    int retVal = decoder.processBinary(ptr, binary.size(), ptmFile);
    EXPECT_EQ(0, retVal);

    std::string expectedOutput = "ProgramBinaryHeader:\n\t4 Magic 1229870147\n\t4 Version 0\n\t4 Device 0\n\t4 GPUPointerSizeInBytes 0\n\t4 NumberOfKernels 1\n\t4 SteppingId 0\n\t4 PatchListSize 0\nKernel #0\nKernelBinaryHeader:\n\t4 CheckSum 4294967295\n\t8 ShaderHashCode 18446744073709551615\n\t4 KernelNameSize 14\n\t4 PatchListSize 0\n\t4 KernelHeapSize 0\n\t4 GeneralStateHeapSize 0\n\t4 DynamicStateHeapSize 0\n\t4 SurfaceStateHeapSize 0\n\t4 KernelUnpaddedSize 0\n\tKernelName ExampleKernel\n";
    EXPECT_EQ(expectedOutput, ptmFile.str());
}

TEST(DecoderTests, givenBinaryWithKernelBinaryHeaderWhenAtLeastOneOfTheKernelSizesExceedSectionSizeThenAbort) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });

    VariableBackup oclocAbortBackup{&abortOclocExecution, &abortOclocExecutionMock};
    std::string kernelName("ExampleKernel");
    auto kernelHeader = createKernelBinaryHeaderCommon(static_cast<uint32_t>(kernelName.size() + 1), 0);
    auto sectionSize = 1234u;
    kernelHeader.DynamicStateHeapSize = sectionSize + 1;

    std::stringstream binarySS;
    binarySS.write(reinterpret_cast<char *>(&kernelHeader), sizeof(SKernelBinaryHeaderCommon));
    binarySS.write(kernelName.c_str(), kernelHeader.KernelNameSize);

    std::stringstream ptmFile;
    MockDecoder decoder{false};
    decoder.mockArgHelper->messagePrinter.setSuppressMessages(true);
    decoder.pathToDump = "non_existing_folder/";
    decoder.parseTokens();
    decoder.mockArgHelper->messagePrinter.setSuppressMessages(false);

    std::string binaryString = binarySS.str();
    std::vector<unsigned char> binary(binaryString.begin(), binaryString.end());
    const void *ptr = reinterpret_cast<void *>(binary.data());

    StdoutCapturer capturer{};
    EXPECT_ANY_THROW(decoder.processKernel(ptr, sectionSize, ptmFile));
    const auto output{capturer.acquireOutput()};
    EXPECT_EQ("Error! DynamicStateHeapSize loaded from KernelBinaryHeader is invalid: 1235.\n", output);
}

TEST(DecoderTests, GivenValidBinaryWhenProcessingBinaryThenProgramAndKernelAndPatchTokensAreReadCorrectly) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });

    std::stringstream binarySS;

    // ProgramBinaryHeader
    auto programHeader = createProgramBinaryHeader(1, 30);
    binarySS.write(reinterpret_cast<const char *>(&programHeader), sizeof(SProgramBinaryHeader));

    // PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO
    SPatchAllocateConstantMemorySurfaceProgramBinaryInfo patchAllocateConstantMemory;
    patchAllocateConstantMemory.Token = 42;
    patchAllocateConstantMemory.Size = 16;
    patchAllocateConstantMemory.ConstantBufferIndex = 0;
    patchAllocateConstantMemory.InlineDataSize = 14;
    binarySS.write(reinterpret_cast<const char *>(&patchAllocateConstantMemory), sizeof(patchAllocateConstantMemory));
    // InlineData
    for (uint8_t i = 0; i < 14; ++i) {
        binarySS.write(reinterpret_cast<char *>(&i), sizeof(uint8_t));
    }

    // KernelBinaryHeader
    std::string kernelName("ExampleKernel");
    auto kernelHeader = createKernelBinaryHeaderCommon(static_cast<uint32_t>(kernelName.size() + 1), 12);

    binarySS.write(reinterpret_cast<const char *>(&kernelHeader), sizeof(SKernelBinaryHeaderCommon));
    binarySS.write(kernelName.c_str(), kernelHeader.KernelNameSize);

    // PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD
    SPatchMediaInterfaceDescriptorLoad patchMediaInterfaceDescriptorLoad;
    patchMediaInterfaceDescriptorLoad.Token = 19;
    patchMediaInterfaceDescriptorLoad.Size = 12;
    patchMediaInterfaceDescriptorLoad.InterfaceDescriptorDataOffset = 0;
    binarySS.write(reinterpret_cast<const char *>(&patchMediaInterfaceDescriptorLoad), sizeof(SPatchMediaInterfaceDescriptorLoad));

    std::string binaryString = binarySS.str();
    std::vector<char> binary(binaryString.begin(), binaryString.end());
    std::stringstream ptmFile;
    MockDecoder decoder;
    decoder.pathToDump = "non_existing_folder/";
    decoder.parseTokens();

    const void *ptr = reinterpret_cast<void *>(binary.data());
    int retVal = decoder.processBinary(ptr, binary.size(), ptmFile);
    EXPECT_EQ(0, retVal);

    std::string expectedOutput = "ProgramBinaryHeader:\n\t4 Magic 1229870147\n\t4 Version 0\n\t4 Device 0\n\t4 GPUPointerSizeInBytes 0\n\t4 NumberOfKernels 1\n\t4 SteppingId 0\n\t4 PatchListSize 30\nPATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO:\n\t4 Token 42\n\t4 Size 16\n\t4 ConstantBufferIndex 0\n\t4 InlineDataSize 14\n\tHex 0 1 2 3 4 5 6 7 8 9 a b c d\nKernel #0\nKernelBinaryHeader:\n\t4 CheckSum 4294967295\n\t8 ShaderHashCode 18446744073709551615\n\t4 KernelNameSize 14\n\t4 PatchListSize 12\n\t4 KernelHeapSize 0\n\t4 GeneralStateHeapSize 0\n\t4 DynamicStateHeapSize 0\n\t4 SurfaceStateHeapSize 0\n\t4 KernelUnpaddedSize 0\n\tKernelName ExampleKernel\nUnidentified PatchToken:\n\t4 Token 19\n\t4 Size 12\n\tHex 0 0 0 0\n";
    EXPECT_EQ(expectedOutput, ptmFile.str());
    EXPECT_TRUE(decoder.getMockIga()->disasmWasCalled);
    EXPECT_FALSE(decoder.getMockIga()->asmWasCalled);
}

TEST(DecoderTests, givenNonPatchtokensBinaryFormatWhenTryingToGetDevBinaryFormatThenDoNotReturnRawData) {
    MockDecoder decoder;
    std::map<std::string, std::string> files;
    auto mockArgHelper = std::make_unique<MockOclocArgHelper>(files);
    decoder.argHelper = mockArgHelper.get();
    files["mockgen.gen"] = "NOTMAGIC\n\n\n\n\n\n\n";
    decoder.binaryFile = "mockgen.gen";
    auto [data, size] = decoder.getDevBinary();
    EXPECT_EQ(nullptr, data);
    EXPECT_EQ(0u, size);
}

TEST(DecoderTests, givenPatchtokensBinaryFormatWhenTryingToGetDevBinaryThenRawDataIsReturned) {
    MockDecoder decoder;
    std::map<std::string, std::string> files;
    auto mockArgHelper = std::make_unique<MockOclocArgHelper>(files);
    decoder.argHelper = mockArgHelper.get();
    size_t dataSize = 11u;
    files["mockgen.gen"] = "CTNI\n\n\n\n\n\n\n";
    decoder.binaryFile = "mockgen.gen";
    auto [binaryData, binarySize] = decoder.getDevBinary();
    std::string dataString(static_cast<const char *>(binaryData), dataSize);
    EXPECT_STREQ("CTNI\n\n\n\n\n\n\n", dataString.c_str());
    EXPECT_EQ(dataSize, binarySize);
}

TEST(DecoderHelperTest, GivenTextSeparatedByTabsWhenSearchingForExistingTextThenItsIndexIsReturned) {
    const std::vector<std::string> lines = {"Some\tNice\tText"};
    const auto position = findPos(lines, "Nice");

    EXPECT_EQ(0u, position);
}

TEST(DecoderHelperTest, GivenTextSeparatedByNewLinesWhenSearchingForExistingTextThenItsIndexIsReturned) {
    const std::vector<std::string> lines = {"Some\nNice\nText"};
    const auto position = findPos(lines, "Nice");

    EXPECT_EQ(0u, position);
}

TEST(DecoderHelperTest, GivenTextSeparatedByCarriageReturnWhenSearchingForExistingTextThenItsIndexIsReturned) {
    const std::vector<std::string> lines = {"Some\rNice\rText"};
    const auto position = findPos(lines, "Nice");

    EXPECT_EQ(0u, position);
}

TEST(DecoderHelperTest, GivenOnlyMatchingSubstringWhenSearchingForExistingTextThenInvalidIndexIsReturned) {
    const std::vector<std::string> lines = {"Carpet"};
    const auto position = findPos(lines, "Car");

    EXPECT_EQ(lines.size(), position);
}

TEST(DecoderHelperTest, GivenPathEndedBySlashWhenCallingAddSlashThenNothingIsDone) {
    std::string path{"./some/path/"};
    addSlash(path);

    EXPECT_EQ("./some/path/", path);
}

TEST(DecoderHelperTest, GivenPathEndedByBackSlashWhenCallingAddSlashThenNothingIsDone) {
    std::string path{".\\some\\path\\"};
    addSlash(path);

    EXPECT_EQ(".\\some\\path\\", path);
}

TEST(DecoderHelperTest, GivenGfxCoreFamilyWhenTranslatingToIgaGenBaseThenExpectedIgaGenBaseIsReturned) {
    constexpr static std::array translations = {
        std::pair{IGFX_GEN12LP_CORE, IGA_XE},
        std::pair{IGFX_XE_HP_CORE, IGA_XE_HP},
        std::pair{IGFX_XE_HPG_CORE, IGA_XE_HPG},
        std::pair{IGFX_XE_HPC_CORE, IGA_XE_HPC},
        std::pair{IGFX_XE2_HPG_CORE, IGA_XE2},
        std::pair{IGFX_XE3_CORE, IGA_XE3},

        std::pair{IGFX_UNKNOWN_CORE, IGA_GEN_INVALID}};

    for (const auto &[input, expectedOutput] : translations) {
        EXPECT_EQ(expectedOutput, translateToIgaGen(input));
    }
}

TEST(DecoderHelperTest, GivenProductFamilyWhenTranslatingToIgaGenBaseThenExpectedIgaGenBaseIsReturned) {
    constexpr static std::array translations = {
        std::pair{IGFX_TIGERLAKE_LP, IGA_XE},
        std::pair{IGFX_ROCKETLAKE, IGA_XE},
        std::pair{IGFX_ALDERLAKE_N, IGA_XE},
        std::pair{IGFX_ALDERLAKE_P, IGA_XE},
        std::pair{IGFX_ALDERLAKE_S, IGA_XE},
        std::pair{IGFX_DG1, IGA_XE},
        std::pair{IGFX_DG2, IGA_XE_HPG},
        std::pair{IGFX_PVC, IGA_XE_HPC},
        std::pair{IGFX_METEORLAKE, IGA_XE_HPG},
        std::pair{IGFX_ARROWLAKE, IGA_XE_HPG},
        std::pair{IGFX_BMG, IGA_XE2},
        std::pair{IGFX_LUNARLAKE, IGA_XE2},
        std::pair{IGFX_PTL, IGA_XE3},

        std::pair{IGFX_UNKNOWN, IGA_GEN_INVALID}};

    for (const auto &[input, expectedOutput] : translations) {
        EXPECT_EQ(expectedOutput, translateToIgaGen(input));
    }
}

} // namespace NEO