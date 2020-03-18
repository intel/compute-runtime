/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/string.h"
#include "shared/test/unit_test/device_binary_format/patchtokens_tests.h"

#include "opencl/test/unit_test/helpers/test_files.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "compiler_options.h"
#include "gtest/gtest.h"

#include <cstring>

using namespace NEO;

class ProcessElfBinaryTests : public ::testing::Test {
  public:
    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
        program = std::make_unique<MockProgram>(*device->getExecutionEnvironment());
        program->pDevice = &device->getDevice();
    }

    std::unique_ptr<MockProgram> program;
    std::unique_ptr<ClDevice> device;
};

TEST_F(ProcessElfBinaryTests, NullBinary) {
    cl_int retVal = program->createProgramFromBinary(nullptr, 0);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

TEST_F(ProcessElfBinaryTests, InvalidBinary) {
    char pBinary[] = "thisistotallyinvalid\0";
    size_t binarySize = strnlen_s(pBinary, 21);
    cl_int retVal = program->createProgramFromBinary(pBinary, binarySize);

    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

TEST_F(ProcessElfBinaryTests, ValidBinary) {
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd16_", ".bin");

    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    cl_int retVal = program->createProgramFromBinary(pBinary.get(), binarySize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary.get(), program->packedDeviceBinary.get(), binarySize));
}

TEST_F(ProcessElfBinaryTests, ValidSpirvBinary) {
    //clCreateProgramWithIL => SPIR-V stored as source code
    const uint32_t spirvBinary[2] = {0x03022307, 0x07230203};
    size_t spirvBinarySize = sizeof(spirvBinary);

    //clCompileProgram => SPIR-V stored as IR binary
    program->isSpirV = true;
    program->irBinary = makeCopy(spirvBinary, spirvBinarySize);
    program->irBinarySize = spirvBinarySize;
    program->programBinaryType = CL_PROGRAM_BINARY_TYPE_LIBRARY;
    EXPECT_NE(nullptr, program->irBinary);
    EXPECT_NE(0u, program->irBinarySize);
    EXPECT_TRUE(program->getIsSpirV());

    //clGetProgramInfo => SPIR-V stored as ELF binary
    cl_int retVal = program->packDeviceBinary();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, program->packedDeviceBinary);
    EXPECT_NE(0u, program->packedDeviceBinarySize);

    //use ELF reader to parse and validate ELF binary
    std::string decodeErrors;
    std::string decodeWarnings;
    auto elf = NEO::Elf::decodeElf(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(program->packedDeviceBinary.get()), program->packedDeviceBinarySize), decodeErrors, decodeWarnings);
    auto header = elf.elfFileHeader;
    ASSERT_NE(nullptr, header);

    //check if ELF binary contains section SECTION_HEADER_TYPE_SPIRV
    bool hasSpirvSection = false;
    for (const auto &elfSectionHeader : elf.sectionHeaders) {
        if (elfSectionHeader.header->type == NEO::Elf::SHT_OPENCL_SPIRV) {
            hasSpirvSection = true;
            break;
        }
    }
    EXPECT_TRUE(hasSpirvSection);

    //clCreateProgramWithBinary => new program should recognize SPIR-V binary
    program->isSpirV = false;
    auto elfBinary = makeCopy(program->packedDeviceBinary.get(), program->packedDeviceBinarySize);
    retVal = program->createProgramFromBinary(elfBinary.get(), program->packedDeviceBinarySize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(program->getIsSpirV());
}

unsigned int BinaryTypeValues[] = {
    CL_PROGRAM_BINARY_TYPE_EXECUTABLE,
    CL_PROGRAM_BINARY_TYPE_LIBRARY,
    CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT};

class ProcessElfBinaryTestsWithBinaryType : public ::testing::TestWithParam<unsigned int> {
  public:
    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
        program = std::make_unique<MockProgram>(*device->getExecutionEnvironment());
        program->pDevice = &device->getDevice();
    }

    std::unique_ptr<MockProgram> program;
    std::unique_ptr<ClDevice> device;
};

TEST_P(ProcessElfBinaryTestsWithBinaryType, GivenBinaryTypeWhenResolveProgramThenProgramIsProperlyResolved) {
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd16_", ".bin");

    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    cl_int retVal = program->createProgramFromBinary(pBinary.get(), binarySize);
    auto options = program->options;
    auto genBinary = makeCopy(program->unpackedDeviceBinary.get(), program->unpackedDeviceBinarySize);
    auto genBinarySize = program->unpackedDeviceBinarySize;
    auto irBinary = makeCopy(program->irBinary.get(), program->irBinarySize);
    auto irBinarySize = program->irBinarySize;

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(binarySize, program->packedDeviceBinarySize);
    EXPECT_EQ(0, memcmp(pBinary.get(), program->packedDeviceBinary.get(), binarySize));

    // delete program's elf reference to force a resolve
    program->packedDeviceBinary.reset();
    program->packedDeviceBinarySize = 0U;
    program->programBinaryType = GetParam();
    retVal = program->packDeviceBinary();
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, program->packedDeviceBinary);

    std::string decodeErrors;
    std::string decodeWarnings;
    auto elf = NEO::Elf::decodeElf(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(program->packedDeviceBinary.get()), program->packedDeviceBinarySize), decodeErrors, decodeWarnings);
    ASSERT_NE(nullptr, elf.elfFileHeader);
    ArrayRef<const uint8_t> decodedIr;
    ArrayRef<const uint8_t> decodedDeviceBinary;
    ArrayRef<const uint8_t> decodedOptions;
    for (auto &section : elf.sectionHeaders) {
        switch (section.header->type) {
        default:
            break;
        case NEO::Elf::SHT_OPENCL_LLVM_BINARY:
            decodedIr = section.data;
            break;
        case NEO::Elf::SHT_OPENCL_SPIRV:
            decodedIr = section.data;
            break;
        case NEO::Elf::SHT_OPENCL_DEV_BINARY:
            decodedDeviceBinary = section.data;
            break;
        case NEO::Elf::SHT_OPENCL_OPTIONS:
            decodedDeviceBinary = section.data;
            break;
        }
    }
    ASSERT_EQ(options.size(), decodedOptions.size());
    ASSERT_EQ(genBinarySize, decodedDeviceBinary.size());
    ASSERT_EQ(irBinarySize, decodedIr.size());

    EXPECT_EQ(0, memcmp(genBinary.get(), decodedDeviceBinary.begin(), genBinarySize));
    EXPECT_EQ(0, memcmp(irBinary.get(), decodedIr.begin(), irBinarySize));
}

INSTANTIATE_TEST_CASE_P(ResolveBinaryTests,
                        ProcessElfBinaryTestsWithBinaryType,
                        ::testing::ValuesIn(BinaryTypeValues));

TEST_F(ProcessElfBinaryTests, BackToBack) {
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd16_", ".bin");

    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    cl_int retVal = program->createProgramFromBinary(pBinary.get(), binarySize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary.get(), program->packedDeviceBinary.get(), binarySize));

    std::string filePath2;
    retrieveBinaryKernelFilename(filePath2, "simple_arg_int_", ".bin");

    pBinary = loadDataFromFile(filePath2.c_str(), binarySize);
    retVal = program->createProgramFromBinary(pBinary.get(), binarySize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary.get(), program->packedDeviceBinary.get(), binarySize));
}

TEST_F(ProcessElfBinaryTests, BuildOptionsEmpty) {
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "simple_kernels_", ".bin");

    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    cl_int retVal = program->createProgramFromBinary(pBinary.get(), binarySize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    const auto &options = program->getOptions();
    size_t optionsSize = strlen(options.c_str()) + 1;
    EXPECT_EQ(0, memcmp("", options.c_str(), optionsSize));
}

TEST_F(ProcessElfBinaryTests, BuildOptionsNotEmpty) {
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "simple_kernels_opts_", ".bin");

    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    cl_int retVal = program->createProgramFromBinary(pBinary.get(), binarySize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    const auto &options = program->getOptions();
    std::string buildOptionsNotEmpty = CompilerOptions::concatenate(CompilerOptions::optDisable, "-DDEF_WAS_SPECIFIED=1");
    EXPECT_STREQ(buildOptionsNotEmpty.c_str(), options.c_str());
}

TEST_F(ProcessElfBinaryTests, GivenBinaryWhenIncompatiblePatchtokenVerionThenProramCreationFails) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    {
        NEO::Elf::ElfEncoder<> elfEncoder;
        elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
        elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_DEV_BINARY, NEO::Elf::SectionNamesOpenCl::deviceBinary, programTokens.storage);
        auto elfBinary = elfEncoder.encode();
        cl_int retVal = program->createProgramFromBinary(elfBinary.data(), elfBinary.size());
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    {
        programTokens.headerMutable->Version -= 1;
        NEO::Elf::ElfEncoder<> elfEncoder;
        elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
        elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_DEV_BINARY, NEO::Elf::SectionNamesOpenCl::deviceBinary, programTokens.storage);
        auto elfBinary = elfEncoder.encode();
        cl_int retVal = program->createProgramFromBinary(elfBinary.data(), elfBinary.size());
        EXPECT_EQ(CL_INVALID_BINARY, retVal);
    }
}
