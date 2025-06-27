/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/common/helpers/stream_capture.h"

#include "opencl/test/unit_test/offline_compiler/environment.h"

#include "gtest/gtest.h"
#include "mock/mock_encoder.h"

#include <array>
#include <cstdint>
#include <sstream>

extern Environment *gEnvironment;

namespace NEO {

TEST(EncoderTests, WhenParsingValidListOfParametersThenReturnValueIsZero) {
    const std::vector<std::string> args = {
        "ocloc",
        "asm",
        "-dump",
        "test_files/dump",
        "-out",
        "test_files/binary_gen.bin"};

    MockEncoder encoder;
    EXPECT_EQ(0, encoder.validateInput(args));
}

TEST(EncoderTests, GivenFlagsWhichRequireMoreArgsWithoutThemWhenParsingThenErrorIsReported) {
    const std::array<std::string, 3> flagsToTest = {
        "-dump", "-device", "-out"};

    for (const auto &flag : flagsToTest) {
        const std::vector<std::string> args = {
            "ocloc",
            "asm",
            flag};

        constexpr auto suppressMessages{false};
        MockEncoder encoder{suppressMessages};

        StreamCapture capture;
        capture.captureStdout();
        const auto result = encoder.validateInput(args);
        const auto output{capture.getCapturedStdout()};

        EXPECT_EQ(-1, result);

        const std::string expectedErrorMessage{"Unknown argument " + flag + "\n"};
        EXPECT_EQ(expectedErrorMessage, output);
    }
}

TEST(EncoderTests, GivenIgnoreIsaPaddingFlagWhenParsingValidListOfParametersThenReturnValueIsZeroAndInternalFlagIsSet) {
    const std::vector<std::string> args = {
        "ocloc",
        "asm",
        "-dump",
        "test_files/dump",
        "-out",
        "test_files/binary_gen.bin",
        "-ignore_isa_padding"};

    MockEncoder encoder;
    EXPECT_EQ(0, encoder.validateInput(args));
    EXPECT_TRUE(encoder.ignoreIsaPadding);
}

TEST(EncoderTests, GivenQuietModeFlagWhenParsingValidListOfParametersThenReturnValueIsZeroAndMessagesAreSuppressed) {
    const std::vector<std::string> args = {
        "ocloc",
        "asm",
        "-dump",
        "test_files/dump",
        "-out",
        "test_files/binary_gen.bin",
        "-q"};

    constexpr auto suppressMessages{false};
    MockEncoder encoder{suppressMessages};

    EXPECT_EQ(0, encoder.validateInput(args));
    EXPECT_TRUE(encoder.argHelper->getPrinterRef().isSuppressed());
}

TEST(EncoderTests, GivenMissingDumpFlagAndArgHelperOutputEnabledWhenParsingValidListOfParametersThenReturnValueIsZeroAndDefaultDirectoryIsNotUsedAsDumpPath) {
    constexpr auto suppressMessages{false};
    MockEncoder encoder{suppressMessages};

    if (gEnvironment->productConfig.empty() || !encoder.getMockIga()->isValidPlatform()) {
        GTEST_SKIP();
    }
    const std::vector<std::string> args = {
        "ocloc",
        "asm",
        "-out",
        "test_files/binary_gen.bin",
        "-device",
        gEnvironment->productConfig.c_str(),
    };

    encoder.mockArgHelper->hasOutput = true;

    StreamCapture capture;
    capture.captureStdout();
    const auto result = encoder.validateInput(args);
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(0, result);
    EXPECT_TRUE(output.empty()) << output;
    EXPECT_TRUE(encoder.pathToDump.empty()) << encoder.pathToDump;

    encoder.mockArgHelper->hasOutput = false;
}

TEST(EncoderTests, GivenMissingPTMFileWhenEncodingThenErrorIsReturnedAndLogIsPrinted) {
    constexpr auto suppressMessages{false};
    MockEncoder encoder{suppressMessages};

    StreamCapture capture;
    capture.captureStdout();
    const auto result = encoder.encode();
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(-1, result);
    EXPECT_EQ("Error! Couldn't find PTM.txt", output);
}

TEST(EncoderTests, GivenMissingSourceFileWhenTryingToCopyBinaryThenErrorIsReturned) {
    MockEncoder encoder;
    encoder.callBaseCopyBinaryToBinary = true;

    std::stringstream outputStream;
    EXPECT_FALSE(encoder.copyBinaryToBinary("bad_source.bin", outputStream, nullptr));
}

TEST(EncoderTests, GivenValidSourceFileWhenTryingToCopyBinaryThenItIsCopied) {
    MockEncoder encoder;
    encoder.callBaseCopyBinaryToBinary = true;
    encoder.filesMap["good_source.bin"] = "TEXT!";

    std::stringstream outputStream;
    ASSERT_TRUE(encoder.copyBinaryToBinary("good_source.bin", outputStream, nullptr));
    EXPECT_EQ("TEXT!", outputStream.str());
}

TEST(EncoderTests, GivenValidSourceFileAndOutputLengthArgumentWhenTryingToCopyBinaryThenItIsCopiedAndLengthIsSet) {
    MockEncoder encoder;
    encoder.callBaseCopyBinaryToBinary = true;
    encoder.filesMap["good_source.bin"] = "TEXT!";

    std::stringstream outputStream;
    uint32_t outputLength{};
    ASSERT_TRUE(encoder.copyBinaryToBinary("good_source.bin", outputStream, &outputLength));

    EXPECT_EQ("TEXT!", outputStream.str());
    EXPECT_EQ(5u, outputLength);
}

TEST(EncoderTests, WhenMissingParametersThenErrorCodeIsReturned) {
    std::vector<std::string> args = {
        "ocloc",
        "asm",
        "-dump",
        "test_files/dump",
        "-out"};

    MockEncoder encoder;
    EXPECT_NE(0, encoder.validateInput(args));
}

TEST(EncoderTests, GivenWrongParametersWhenParsingParametersThenErrorCodeIsReturned) {
    std::vector<std::string> args = {
        "ocloc",
        "asm",
        "-dump",
        "",
        "-out",
        "rasputin"};

    MockEncoder encoder;
    EXPECT_NE(0, encoder.validateInput(args));
}

TEST(EncoderTests, WhenTryingToCopyNonExistingFileThenErrorCodeIsReturned) {
    MockEncoder encoder;
    std::stringstream ss;
    auto retVal = encoder.copyBinaryToBinary("test_files/non_existing.bin", ss);
    EXPECT_FALSE(retVal);
}

TEST(EncoderTests, WhenWritingValuesToBinaryThenValuesAreWrittenCorrectly) {
    MockEncoder encoder;
    std::stringstream in;
    std::stringstream out;
    in.str("255 255 255 255");

    std::string s = in.str();
    encoder.write<uint8_t>(in, out);
    uint8_t val1;
    out.read(reinterpret_cast<char *>(&val1), sizeof(uint8_t));
    ASSERT_EQ(static_cast<uint8_t>(255), val1);
    encoder.write<uint16_t>(in, out);
    uint16_t val2;
    out.read(reinterpret_cast<char *>(&val2), sizeof(uint16_t));
    ASSERT_EQ(static_cast<uint16_t>(255), val2);
    encoder.write<uint32_t>(in, out);
    uint32_t val3;
    out.read(reinterpret_cast<char *>(&val3), sizeof(uint32_t));
    ASSERT_EQ(static_cast<uint32_t>(255), val3);
    encoder.write<uint64_t>(in, out);
    uint64_t val4;
    out.read(reinterpret_cast<char *>(&val4), sizeof(uint64_t));
    ASSERT_EQ(static_cast<uint64_t>(255), val4);
}

TEST(EncoderTests, GivenProperPTMFileFormatWhenWritingToBinaryThenValuesAreWrittenCorrectly) {
    MockEncoder encoder;
    std::stringstream out;
    out.str("");
    std::string s = "ProgramBinaryHeader:";
    int retVal = encoder.writeDeviceBinary(s, out);
    ASSERT_EQ(0, retVal);
    ASSERT_EQ("", out.str());

    s = "Hex 48 65 6c 6c 6f 20 77 6f 72 6c 64";
    retVal = encoder.writeDeviceBinary(s, out);
    ASSERT_EQ(0, retVal);
    ASSERT_EQ("Hello world", out.str());
    s = "1 CheckOne 220";

    out.str("");
    retVal = encoder.writeDeviceBinary(s, out);
    ASSERT_EQ(0, retVal);
    uint8_t val1;
    out.read(reinterpret_cast<char *>(&val1), sizeof(uint8_t));
    ASSERT_EQ(static_cast<uint8_t>(220), val1);

    s = "2 CheckTwo 2428";
    out.str("");
    retVal = encoder.writeDeviceBinary(s, out);
    ASSERT_EQ(0, retVal);
    uint16_t val2;
    out.read(reinterpret_cast<char *>(&val2), sizeof(uint16_t));
    ASSERT_EQ(static_cast<uint16_t>(2428), val2);

    s = "4 CheckThree 242806820";
    out.str("");
    retVal = encoder.writeDeviceBinary(s, out);
    ASSERT_EQ(retVal, 0);
    uint32_t val3;
    out.read(reinterpret_cast<char *>(&val3), sizeof(uint32_t));
    ASSERT_EQ(static_cast<uint32_t>(242806820), val3);

    s = "8 CheckFour 242806820";
    out.str("");
    retVal = encoder.writeDeviceBinary(s, out);
    ASSERT_EQ(retVal, 0);
    uint64_t val4;
    out.read(reinterpret_cast<char *>(&val4), sizeof(uint64_t));
    ASSERT_EQ(static_cast<uint64_t>(242806820), val4);
}

TEST(EncoderTests, GivenImproperPTMFIleFormatWhenWritingToBinaryThenErrorCodeIsReturned) {
    std::string s = "3 UnknownSize 41243";
    std::stringstream out("");
    MockEncoder encoder;
    int retVal = encoder.writeDeviceBinary(s, out);
    ASSERT_EQ(-1, retVal);
}
TEST(EncoderTests, GivenIncorrectPatchListSizeWhileCalculatingPatchListSizeThenPatchListSizeIsSetToCorrectValue) {
    std::vector<std::string> ptmFile;
    ptmFile.push_back("ProgramBinaryHeader:");
    ptmFile.push_back("\t4 Magic 1229870147");
    ptmFile.push_back("\t4 PatchListSize 14");
    ptmFile.push_back("PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO:");
    ptmFile.push_back("\t8 Token 42");
    ptmFile.push_back("\t4 Size 16");
    ptmFile.push_back("\t1 ConstantBufferIndex 0");
    ptmFile.push_back("\t2 InlineDataSize 14");
    ptmFile.push_back("\tHex 48 65 6c 6c 6f 20 77 6f 72 6c 64 21 a 0");
    ptmFile.push_back("Kernel #0");
    MockEncoder encoder;
    encoder.calculatePatchListSizes(ptmFile);
    EXPECT_EQ("\t4 PatchListSize 29", ptmFile[2]);
}

TEST(EncoderTests, GivenCorrectPTMFileWhileProcessingThenCorrectProgramHeaderExpected) {
    std::stringstream expectedBinary;
    uint8_t byte;
    uint32_t byte4;

    byte4 = 1229870147;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 1042;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 12;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 4;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 1;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 2;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 18;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 42;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 16;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 0;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 2;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte = 0x48;
    expectedBinary.write(reinterpret_cast<char *>(&byte), sizeof(uint8_t));
    byte = 0x65;
    expectedBinary.write(reinterpret_cast<char *>(&byte), sizeof(uint8_t));

    std::vector<std::string> ptmFile;
    ptmFile.push_back("ProgramBinaryHeader:");
    ptmFile.push_back("\t4 Magic 1229870147");
    ptmFile.push_back("\t4 Version 1042");
    ptmFile.push_back("\t4 Device 12");
    ptmFile.push_back("\t4 GPUPointerSizeInBytes 4");
    ptmFile.push_back("\t4 NumberOfKernels 1");
    ptmFile.push_back("\t4 SteppingId 2");
    ptmFile.push_back("\t4 PatchListSize 18");
    ptmFile.push_back("PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO:");
    ptmFile.push_back("\t4 Token 42");
    ptmFile.push_back("\t4 Size 16");
    ptmFile.push_back("\t4 ConstantBufferIndex 0");
    ptmFile.push_back("\t4 InlineDataSize 2");
    ptmFile.push_back("\tHex 48 65");
    ptmFile.push_back("Kernel #0");
    ptmFile.push_back("KernelBinaryHeader:");
    ptmFile.push_back("\t4 CheckSum 2316678223");
    ptmFile.push_back("\t8 ShaderHashCode 4988534869940066475");
    ptmFile.push_back("\t4 KernelNameSize 12");
    ptmFile.push_back("\t4 PatchListSize 0");
    ptmFile.push_back("\t4 KernelHeapSize 0");
    ptmFile.push_back("\t4 GeneralStateHeapSize 0");
    ptmFile.push_back("\t4 DynamicStateHeapSize 0");
    ptmFile.push_back("\t4 KernelUnpaddedSize 520");
    ptmFile.push_back("\tKernelName kernel");
    std::stringstream binary;

    MockEncoder().processBinary(ptmFile, binary);
    EXPECT_EQ(expectedBinary.str(), binary.str());
}

TEST(EncoderTests, WhenAddPaddingIsCalledThenProperNumberOfZerosIsAdded) {
    std::stringstream stream;
    stream << "aa";
    MockEncoder().addPadding(stream, 8);
    std::string asString = stream.str();
    ASSERT_EQ(10U, asString.size());
    char expected[] = {'a', 'a', 0, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(0, memcmp(asString.c_str(), expected, 10U));
}

TEST(EncoderTests, WhenProcessingDeviceBinaryThenProperChecksumIsCalculated) {
    std::stringstream expectedBinary;
    uint8_t byte;
    uint32_t byte4;
    uint64_t byte8;

    MockEncoder encoder;
    std::string kernelName = "kernel";
    encoder.filesMap["kernel_DynamicStateHeap.bin"] = std::string(16, 2);
    encoder.filesMap["kernel_KernelHeap.dat"] = std::string(16, 4);
    encoder.filesMap["kernel_SurfaceStateHeap.bin"] = std::string(16, 8);
    std::stringstream kernelBlob;
    kernelBlob << kernelName;
    kernelBlob.write(encoder.filesMap["kernel_KernelHeap.dat"].data(), encoder.filesMap["kernel_KernelHeap.dat"].size());
    encoder.addPadding(kernelBlob, 128);                                                                // isa prefetch padding
    encoder.addPadding(kernelBlob, 64 - (encoder.filesMap["kernel_KernelHeap.dat"].size() + 128) % 64); // isa alignment
    size_t kernelHeapSize = encoder.filesMap["kernel_KernelHeap.dat"].size();
    kernelHeapSize = alignUp(kernelHeapSize + 128, 64);
    kernelBlob.write(encoder.filesMap["kernel_DynamicStateHeap.bin"].data(), encoder.filesMap["kernel_DynamicStateHeap.bin"].size());
    kernelBlob.write(encoder.filesMap["kernel_SurfaceStateHeap.bin"].data(), encoder.filesMap["kernel_SurfaceStateHeap.bin"].size());

    auto kernelBlobData = kernelBlob.str();
    uint64_t hashValue = NEO::Hash::hash(reinterpret_cast<const char *>(kernelBlobData.data()), kernelBlobData.size());
    uint32_t checksum = hashValue & 0xFFFFFFFF;

    byte4 = 1229870147;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 1042;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 12;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 4;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 1;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 2;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 18;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 42;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 16;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 0;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 2;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte = 0x48;
    expectedBinary.write(reinterpret_cast<char *>(&byte), sizeof(uint8_t));
    byte = 0x65;
    expectedBinary.write(reinterpret_cast<char *>(&byte), sizeof(uint8_t));
    byte4 = checksum;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte8 = 4988534869940066475;
    expectedBinary.write(reinterpret_cast<char *>(&byte8), sizeof(uint64_t));
    byte4 = static_cast<uint32_t>(kernelName.size());
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 0;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = static_cast<uint32_t>(kernelHeapSize);
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 0;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 16;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = static_cast<uint32_t>(encoder.filesMap["kernel_KernelHeap.dat"].size());
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    expectedBinary.write(kernelName.c_str(), kernelName.length());
    expectedBinary.write(encoder.filesMap["kernel_KernelHeap.dat"].data(), encoder.filesMap["kernel_KernelHeap.dat"].size());

    std::vector<std::string> ptmFile;
    ptmFile.push_back("ProgramBinaryHeader:");
    ptmFile.push_back("\t4 Magic 1229870147");
    ptmFile.push_back("\t4 Version 1042");
    ptmFile.push_back("\t4 Device 12");
    ptmFile.push_back("\t4 GPUPointerSizeInBytes 4");
    ptmFile.push_back("\t4 NumberOfKernels 1");
    ptmFile.push_back("\t4 SteppingId 2");
    ptmFile.push_back("\t4 PatchListSize 18");
    ptmFile.push_back("PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO:");
    ptmFile.push_back("\t4 Token 42");
    ptmFile.push_back("\t4 Size 16");
    ptmFile.push_back("\t4 ConstantBufferIndex 0");
    ptmFile.push_back("\t4 InlineDataSize 2");
    ptmFile.push_back("\tHex 48 65");
    ptmFile.push_back("Kernel #0");
    ptmFile.push_back("KernelBinaryHeader:");
    ptmFile.push_back("\t4 CheckSum 0");
    ptmFile.push_back("\t8 ShaderHashCode 4988534869940066475");
    ptmFile.push_back("\t4 KernelNameSize " + std::to_string(kernelName.size()));
    ptmFile.push_back("\t4 PatchListSize 0");
    ptmFile.push_back("\t4 KernelHeapSize 16");
    ptmFile.push_back("\t4 GeneralStateHeapSize 0");
    ptmFile.push_back("\t4 DynamicStateHeapSize 16");
    ptmFile.push_back("\t4 KernelUnpaddedSize 16");
    ptmFile.push_back("\tKernelName " + kernelName);

    std::stringstream result;
    auto ret = encoder.processBinary(ptmFile, result);
    auto resultAsString = result.str();
    EXPECT_EQ(0, ret);
    auto expectedBinaryAsString = expectedBinary.str();
    resultAsString.resize(expectedBinaryAsString.size()); // don't test beyond kernel header
    EXPECT_EQ(expectedBinaryAsString, resultAsString);
    EXPECT_FALSE(encoder.getMockIga()->disasmWasCalled);
    EXPECT_FALSE(encoder.getMockIga()->asmWasCalled);
}

TEST(EncoderTests, WhenProcessingDeviceBinaryAndAsmIsAvailableThenAseembleItWithIga) {
    std::stringstream expectedBinary;
    uint8_t byte;
    uint32_t byte4;
    uint64_t byte8;

    MockEncoder encoder;
    encoder.getMockIga()->binaryToReturn = std::string(32, 13);
    std::string kernelName = "kernel";
    encoder.filesMap["kernel_DynamicStateHeap.bin"] = std::string(16, 2);
    encoder.filesMap["kernel_KernelHeap.dat"] = std::string(16, 4);
    encoder.filesMap["kernel_KernelHeap.asm"] = std::string(16, 7);
    encoder.filesMap["kernel_SurfaceStateHeap.bin"] = std::string(16, 8);
    std::stringstream kernelBlob;
    kernelBlob << kernelName;
    kernelBlob.write(encoder.getMockIga()->binaryToReturn.c_str(), encoder.getMockIga()->binaryToReturn.size());
    encoder.addPadding(kernelBlob, 128);                                                           // isa prefetch padding
    encoder.addPadding(kernelBlob, 64 - (encoder.getMockIga()->binaryToReturn.size() + 128) % 64); // isa alignment
    size_t kernelHeapSize = encoder.getMockIga()->binaryToReturn.size();
    kernelHeapSize = alignUp(kernelHeapSize + 128, 64);
    kernelBlob.write(encoder.filesMap["kernel_DynamicStateHeap.bin"].data(), encoder.filesMap["kernel_DynamicStateHeap.bin"].size());
    kernelBlob.write(encoder.filesMap["kernel_SurfaceStateHeap.bin"].data(), encoder.filesMap["kernel_SurfaceStateHeap.bin"].size());

    auto kernelBlobData = kernelBlob.str();
    uint64_t hashValue = NEO::Hash::hash(reinterpret_cast<const char *>(kernelBlobData.data()), kernelBlobData.size());
    uint32_t checksum = hashValue & 0xFFFFFFFF;

    byte4 = 1229870147;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 1042;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 12;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 4;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 1;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 2;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 18;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 42;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 16;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 0;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 2;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte = 0x48;
    expectedBinary.write(reinterpret_cast<char *>(&byte), sizeof(uint8_t));
    byte = 0x65;
    expectedBinary.write(reinterpret_cast<char *>(&byte), sizeof(uint8_t));
    byte4 = checksum;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte8 = 4988534869940066475;
    expectedBinary.write(reinterpret_cast<char *>(&byte8), sizeof(uint64_t));
    byte4 = static_cast<uint32_t>(kernelName.size());
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 0;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = static_cast<uint32_t>(kernelHeapSize);
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = 0;
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = static_cast<uint32_t>(16);
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    byte4 = static_cast<uint32_t>(encoder.getMockIga()->binaryToReturn.size());
    expectedBinary.write(reinterpret_cast<char *>(&byte4), sizeof(uint32_t));
    expectedBinary.write(kernelName.c_str(), kernelName.length());
    expectedBinary.write(encoder.getMockIga()->binaryToReturn.data(), encoder.getMockIga()->binaryToReturn.size());

    std::vector<std::string> ptmFile;
    ptmFile.push_back("ProgramBinaryHeader:");
    ptmFile.push_back("\t4 Magic 1229870147");
    ptmFile.push_back("\t4 Version 1042");
    ptmFile.push_back("\t4 Device 12");
    ptmFile.push_back("\t4 GPUPointerSizeInBytes 4");
    ptmFile.push_back("\t4 NumberOfKernels 1");
    ptmFile.push_back("\t4 SteppingId 2");
    ptmFile.push_back("\t4 PatchListSize 18");
    ptmFile.push_back("PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO:");
    ptmFile.push_back("\t4 Token 42");
    ptmFile.push_back("\t4 Size 16");
    ptmFile.push_back("\t4 ConstantBufferIndex 0");
    ptmFile.push_back("\t4 InlineDataSize 2");
    ptmFile.push_back("\tHex 48 65");
    ptmFile.push_back("Kernel #0");
    ptmFile.push_back("KernelBinaryHeader:");
    ptmFile.push_back("\t4 CheckSum 0");
    ptmFile.push_back("\t8 ShaderHashCode 4988534869940066475");
    ptmFile.push_back("\t4 KernelNameSize " + std::to_string(kernelName.size()));
    ptmFile.push_back("\t4 PatchListSize 0");
    ptmFile.push_back("\t4 KernelHeapSize 16");
    ptmFile.push_back("\t4 GeneralStateHeapSize 0");
    ptmFile.push_back("\t4 DynamicStateHeapSize 16");
    ptmFile.push_back("\t4 KernelUnpaddedSize 16");
    ptmFile.push_back("\tKernelName " + kernelName);

    std::stringstream result;
    auto ret = encoder.processBinary(ptmFile, result);
    auto resultAsString = result.str();
    EXPECT_EQ(0, ret);
    auto expectedBinaryAsString = expectedBinary.str();
    resultAsString.resize(expectedBinaryAsString.size()); // don't test beyond kernel header
    EXPECT_EQ(expectedBinaryAsString, resultAsString);
    EXPECT_FALSE(encoder.getMockIga()->disasmWasCalled);
    EXPECT_TRUE(encoder.getMockIga()->asmWasCalled);
    EXPECT_EQ(encoder.filesMap["kernel_KernelHeap.asm"], encoder.getMockIga()->receivedAsm);
}

} // namespace NEO