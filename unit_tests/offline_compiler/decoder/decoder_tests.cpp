/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock/mock_decoder.h"
#include "runtime/helpers/array_count.h"
#include "unit_tests/test_files/patch_list.h"

#include "gmock/gmock.h"

#include <fstream>

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

namespace OCLRT {
TEST(DecoderTests, WhenParsingValidListOfParametersThenReturnValueIsZero) {
    const char *argv[] = {
        "cloc",
        "decoder",
        "-file",
        "test_files/binary.bin",
        "-patch",
        "test_files/patch",
        "-dump",
        "test_files/created"};

    MockDecoder decoder;
    EXPECT_EQ(0, decoder.validateInput(static_cast<uint32_t>(arrayCount<const char *>(argv)), argv));
}

TEST(DecoderTests, WhenMissingParametersThenValidateInputReturnsErrorCode) {
    const char *argv[] = {
        "cloc",
        "decoder",
        "-file",
        "test_files/binary.bin",
        "-patch",
        "test_files"};

    MockDecoder decoder;
    EXPECT_NE(0, decoder.validateInput(static_cast<uint32_t>(arrayCount<const char *>(argv)), argv));
}

TEST(DecoderTests, GivenWrongParametersWhenParsingParametersThenValidateInputReturnsErrorCode) {
    const char *argv[] = {
        "cloc",
        "decoder",
        "-file",
        "test_files/no_extension",
        "-patch",
        "test_files",
        "-dump",
        "test_files/created"};

    MockDecoder decoder;
    EXPECT_NE(0, decoder.validateInput(static_cast<uint32_t>(arrayCount<const char *>(argv)), argv));
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
    uint32_t full_size = decoder.readStructFields(lines, pos, fields);
    EXPECT_EQ(static_cast<uint32_t>(15), full_size);

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
    MockDecoder decoder;
    decoder.pathToPatch = "test_files/";
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

    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.patchTokens[2]->size));
    EXPECT_EQ("PATCH_TOKEN_STATE_SIP", (decoder.patchTokens[2]->name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.patchTokens[2]->fields[0].size));
    EXPECT_EQ("SystemKernelOffset", (decoder.patchTokens[2]->fields[0].name));

    EXPECT_EQ(static_cast<uint8_t>(12), decoder.patchTokens[5]->size);
    EXPECT_EQ("PATCH_TOKEN_SAMPLER_STATE_ARRAY", decoder.patchTokens[5]->name);
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.patchTokens[5]->fields[0].size));
    EXPECT_EQ("Offset", (decoder.patchTokens[5]->fields[0].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.patchTokens[5]->fields[1].size));
    EXPECT_EQ("Count", (decoder.patchTokens[5]->fields[1].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.patchTokens[5]->fields[2].size));
    EXPECT_EQ("BorderColorOffset", (decoder.patchTokens[5]->fields[2].name));

    EXPECT_EQ(static_cast<uint8_t>(8), decoder.patchTokens[42]->size);
    EXPECT_EQ("PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO", decoder.patchTokens[42]->name);
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.patchTokens[42]->fields[0].size));
    EXPECT_EQ("ConstantBufferIndex", (decoder.patchTokens[42]->fields[0].name));
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.patchTokens[42]->fields[1].size));
    EXPECT_EQ("InlineDataSize", (decoder.patchTokens[42]->fields[1].name));

    EXPECT_EQ(static_cast<uint8_t>(4), decoder.patchTokens[19]->size);
    EXPECT_EQ("PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD", decoder.patchTokens[19]->name);
    EXPECT_EQ(static_cast<uint8_t>(4), (decoder.patchTokens[19]->fields[0].size));
    EXPECT_EQ("InterfaceDescriptorDataOffset", (decoder.patchTokens[19]->fields[0].name));
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
    auto PTptr = std::make_unique<PatchToken>();
    PTptr->size = 20;
    PTptr->name = "Example patchtoken";
    PTptr->fields.push_back(PTField{4, "First"});
    PTptr->fields.push_back(PTField{4, "Second"});
    decoder.patchTokens.insert(std::pair<uint8_t, std::unique_ptr<PatchToken>>(4, std::move(PTptr)));
    void *ptr = reinterpret_cast<void *>(binary.data());
    decoder.readPatchTokens(ptr, 28, out);
    std::string s = "Example patchtoken:\n\t4 Token 4\n\t4 Size 16\n\t4 First 1234\n\t4 Second 5678\nUnidentified PatchToken:\n\t4 Token 2\n\t4 Size 12\n\tHex ff ff ff ff\n";
    EXPECT_EQ(s, out.str());
}

TEST(DecoderTests, GivenValidBinaryWithoutPatchTokensWhenProcessingBinaryThenBinaryIsReadCorrectly) {

    auto programHeader = createProgramBinaryHeader(1, 0);
    std::string kernelName("ExampleKernel");
    auto kernelHeader = createKernelBinaryHeaderCommon(static_cast<uint32_t>(kernelName.size() + 1), 0);

    std::stringstream binarySS;
    binarySS.write(reinterpret_cast<char *>(&programHeader), sizeof(SProgramBinaryHeader));
    binarySS.write(reinterpret_cast<char *>(&kernelHeader), sizeof(SKernelBinaryHeaderCommon));
    binarySS.write(kernelName.c_str(), kernelHeader.KernelNameSize);

    std::stringstream ptmFile;
    MockDecoder decoder;
    decoder.pathToPatch = "test_files/";
    decoder.pathToDump = "non_existing_folder/";
    decoder.parseTokens();

    std::string binaryString = binarySS.str();
    std::vector<unsigned char> binary(binaryString.begin(), binaryString.end());
    auto ptr = reinterpret_cast<void *>(binary.data());
    int retVal = decoder.processBinary(ptr, ptmFile);
    EXPECT_EQ(0, retVal);

    std::string expectedOutput = "ProgramBinaryHeader:\n\t4 Magic 1229870147\n\t4 Version 0\n\t4 Device 0\n\t4 GPUPointerSizeInBytes 0\n\t4 NumberOfKernels 1\n\t4 SteppingId 0\n\t4 PatchListSize 0\nKernel #0\nKernelBinaryHeader:\n\t4 CheckSum 4294967295\n\t8 ShaderHashCode 18446744073709551615\n\t4 KernelNameSize 14\n\t4 PatchListSize 0\n\t4 KernelHeapSize 0\n\t4 GeneralStateHeapSize 0\n\t4 DynamicStateHeapSize 0\n\t4 SurfaceStateHeapSize 0\n\t4 KernelUnpaddedSize 0\n\tKernelName ExampleKernel\n";
    EXPECT_EQ(expectedOutput, ptmFile.str());
}

TEST(DecoderTests, GivenValidBinaryWhenProcessingBinaryThenProgramAndKernelAndPatchTokensAreReadCorrectly) {
    std::stringstream binarySS;

    //ProgramBinaryHeader
    auto programHeader = createProgramBinaryHeader(1, 30);
    binarySS.write(reinterpret_cast<const char *>(&programHeader), sizeof(SProgramBinaryHeader));

    //PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO
    SPatchAllocateConstantMemorySurfaceProgramBinaryInfo patchAllocateConstantMemory;
    patchAllocateConstantMemory.Token = 42;
    patchAllocateConstantMemory.Size = 16;
    patchAllocateConstantMemory.ConstantBufferIndex = 0;
    patchAllocateConstantMemory.InlineDataSize = 14;
    binarySS.write(reinterpret_cast<const char *>(&patchAllocateConstantMemory), sizeof(patchAllocateConstantMemory));
    //InlineData
    for (uint8_t i = 0; i < 14; ++i) {
        binarySS.write(reinterpret_cast<char *>(&i), sizeof(uint8_t));
    }

    //KernelBinaryHeader
    std::string kernelName("ExampleKernel");
    auto kernelHeader = createKernelBinaryHeaderCommon(static_cast<uint32_t>(kernelName.size() + 1), 12);

    binarySS.write(reinterpret_cast<const char *>(&kernelHeader), sizeof(SKernelBinaryHeaderCommon));
    binarySS.write(kernelName.c_str(), kernelHeader.KernelNameSize);

    //PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD
    SPatchMediaInterfaceDescriptorLoad patchMediaInterfaceDescriptorLoad;
    patchMediaInterfaceDescriptorLoad.Token = 19;
    patchMediaInterfaceDescriptorLoad.Size = 12;
    patchMediaInterfaceDescriptorLoad.InterfaceDescriptorDataOffset = 0;
    binarySS.write(reinterpret_cast<const char *>(&patchMediaInterfaceDescriptorLoad), sizeof(SPatchMediaInterfaceDescriptorLoad));

    std::string binaryString = binarySS.str();
    std::vector<char> binary(binaryString.begin(), binaryString.end());
    std::stringstream ptmFile;
    MockDecoder decoder;
    decoder.pathToPatch = "test_files/";
    decoder.pathToDump = "non_existing_folder/";
    decoder.parseTokens();

    auto ptr = reinterpret_cast<void *>(binary.data());
    int retVal = decoder.processBinary(ptr, ptmFile);
    EXPECT_EQ(0, retVal);

    std::string expectedOutput = "ProgramBinaryHeader:\n\t4 Magic 1229870147\n\t4 Version 0\n\t4 Device 0\n\t4 GPUPointerSizeInBytes 0\n\t4 NumberOfKernels 1\n\t4 SteppingId 0\n\t4 PatchListSize 30\nPATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO:\n\t4 Token 42\n\t4 Size 16\n\t4 ConstantBufferIndex 0\n\t4 InlineDataSize 14\n\tHex 0 1 2 3 4 5 6 7 8 9 a b c d\nKernel #0\nKernelBinaryHeader:\n\t4 CheckSum 4294967295\n\t8 ShaderHashCode 18446744073709551615\n\t4 KernelNameSize 14\n\t4 PatchListSize 12\n\t4 KernelHeapSize 0\n\t4 GeneralStateHeapSize 0\n\t4 DynamicStateHeapSize 0\n\t4 SurfaceStateHeapSize 0\n\t4 KernelUnpaddedSize 0\n\tKernelName ExampleKernel\nPATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD:\n\t4 Token 19\n\t4 Size 12\n\t4 InterfaceDescriptorDataOffset 0\n";
    EXPECT_EQ(expectedOutput, ptmFile.str());
}
} // namespace OCLRT
