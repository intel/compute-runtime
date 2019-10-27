/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/elf/reader.h"
#include "core/helpers/file_io.h"
#include "core/helpers/string.h"
#include "runtime/device/device.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_program.h"

#include "gtest/gtest.h"

#include <cstring>

using namespace NEO;

class ProcessElfBinaryTests : public ::testing::Test {
  public:
    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        program = std::make_unique<MockProgram>(*executionEnvironment);
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockProgram> program;
};

TEST_F(ProcessElfBinaryTests, NullBinary) {
    uint32_t binaryVersion;
    cl_int retVal = program->processElfBinary(nullptr, 0, binaryVersion);

    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    EXPECT_NE(0u, binaryVersion);
}

TEST_F(ProcessElfBinaryTests, InvalidBinary) {
    uint32_t binaryVersion;
    char pBinary[] = "thisistotallyinvalid\0";
    size_t binarySize = strnlen_s(pBinary, 21);
    cl_int retVal = program->processElfBinary(pBinary, binarySize, binaryVersion);

    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    EXPECT_NE(0u, binaryVersion);
}

TEST_F(ProcessElfBinaryTests, ValidBinary) {
    uint32_t binaryVersion;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");

    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    cl_int retVal = program->processElfBinary(pBinary.get(), binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary.get(), program->elfBinary.data(), binarySize));
    EXPECT_NE(0u, binaryVersion);
}

TEST_F(ProcessElfBinaryTests, ValidSpirvBinary) {
    //clCreateProgramWithIL => SPIR-V stored as source code
    const uint32_t spirvBinary[2] = {0x03022307, 0x07230203};
    size_t spirvBinarySize = sizeof(spirvBinary);
    auto isSpirV = Program::isValidSpirvBinary(spirvBinary, spirvBinarySize);
    EXPECT_TRUE(isSpirV);

    //clCompileProgram => SPIR-V stored as IR binary
    program->isSpirV = true;
    program->irBinary = makeCopy(spirvBinary, spirvBinarySize);
    program->irBinarySize = spirvBinarySize;
    program->programBinaryType = CL_PROGRAM_BINARY_TYPE_LIBRARY;
    EXPECT_NE(nullptr, program->irBinary);
    EXPECT_NE(0u, program->irBinarySize);
    EXPECT_TRUE(program->getIsSpirV());

    //clGetProgramInfo => SPIR-V stored as ELF binary
    cl_int retVal = program->resolveProgramBinary();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(program->elfBinary.empty());
    EXPECT_NE(0u, program->elfBinarySize);

    //use ELF reader to parse and validate ELF binary
    CLElfLib::CElfReader elfReader(program->elfBinary);
    const CLElfLib::SElf64Header *elf64Header = elfReader.getElfHeader();
    ASSERT_NE(nullptr, elf64Header);
    EXPECT_EQ(elf64Header->Type, CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_LIBRARY);

    //check if ELF binary contains section SH_TYPE_SPIRV
    bool hasSpirvSection = false;
    for (const auto &elfSectionHeader : elfReader.getSectionHeaders()) {
        if (elfSectionHeader.Type == CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV) {
            hasSpirvSection = true;
            break;
        }
    }
    EXPECT_TRUE(hasSpirvSection);

    //clCreateProgramWithBinary => new program should recognize SPIR-V binary
    program->isSpirV = false;
    uint32_t elfBinaryVersion;
    auto pElfBinary = std::unique_ptr<char>(new char[program->elfBinarySize]);
    memcpy_s(pElfBinary.get(), program->elfBinarySize, program->elfBinary.data(), program->elfBinarySize);
    retVal = program->processElfBinary(pElfBinary.get(), program->elfBinarySize, elfBinaryVersion);
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
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        program = std::make_unique<MockProgram>(*executionEnvironment);
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockProgram> program;
};

TEST_P(ProcessElfBinaryTestsWithBinaryType, GivenBinaryTypeWhenResolveProgramThenProgramIsProperlyResolved) {
    uint32_t binaryVersion;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");

    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    cl_int retVal = program->processElfBinary(pBinary.get(), binarySize, binaryVersion);

    const auto &options = program->getOptions();
    size_t optionsSize = strlen(options.c_str()) + 1;
    auto pTmpGenBinary = new char[program->genBinarySize];
    auto pTmpIrBinary = new char[program->irBinarySize];
    auto pTmpOptions = new char[optionsSize];

    memcpy_s(pTmpGenBinary, program->genBinarySize, program->genBinary.get(), program->genBinarySize);
    memcpy_s(pTmpIrBinary, program->irBinarySize, program->irBinary.get(), program->irBinarySize);
    memcpy_s(pTmpOptions, optionsSize, options.c_str(), optionsSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary.get(), program->elfBinary.data(), binarySize));
    EXPECT_NE(0u, binaryVersion);

    // delete program's elf reference to force a resolve
    program->isProgramBinaryResolved = false;
    program->programBinaryType = GetParam();
    retVal = program->resolveProgramBinary();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pTmpGenBinary, program->genBinary.get(), program->genBinarySize));
    EXPECT_EQ(0, memcmp(pTmpIrBinary, program->irBinary.get(), program->irBinarySize));
    EXPECT_EQ(0, memcmp(pTmpOptions, options.c_str(), optionsSize));

    delete[] pTmpGenBinary;
    delete[] pTmpIrBinary;
    delete[] pTmpOptions;
}

INSTANTIATE_TEST_CASE_P(ResolveBinaryTests,
                        ProcessElfBinaryTestsWithBinaryType,
                        ::testing::ValuesIn(BinaryTypeValues));

TEST_F(ProcessElfBinaryTests, BackToBack) {
    uint32_t binaryVersion;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");

    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    cl_int retVal = program->processElfBinary(pBinary.get(), binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary.get(), program->elfBinary.data(), binarySize));
    EXPECT_NE(0u, binaryVersion);

    std::string filePath2;
    retrieveBinaryKernelFilename(filePath2, "simple_arg_int_", ".bin");

    pBinary = loadDataFromFile(filePath2.c_str(), binarySize);
    retVal = program->processElfBinary(pBinary.get(), binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary.get(), program->elfBinary.data(), binarySize));
    EXPECT_NE(0u, binaryVersion);
}

TEST_F(ProcessElfBinaryTests, BuildOptionsEmpty) {
    uint32_t binaryVersion;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "simple_kernels_", ".bin");

    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    cl_int retVal = program->processElfBinary(pBinary.get(), binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    const auto &options = program->getOptions();
    size_t optionsSize = strlen(options.c_str()) + 1;
    EXPECT_EQ(0, memcmp("", options.c_str(), optionsSize));
    EXPECT_NE(0u, binaryVersion);
}

TEST_F(ProcessElfBinaryTests, BuildOptionsNotEmpty) {
    uint32_t binaryVersion;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "simple_kernels_opts_", ".bin");

    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    cl_int retVal = program->processElfBinary(pBinary.get(), binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    const auto &options = program->getOptions();
    size_t optionsSize = strlen(options.c_str()) + 1;
    std::string buildOptionsNotEmpty = "-cl-opt-disable -DDEF_WAS_SPECIFIED=1";
    EXPECT_EQ(0, memcmp(buildOptionsNotEmpty.c_str(), options.c_str(), optionsSize));
    EXPECT_NE(0u, binaryVersion);
}

TEST_F(ProcessElfBinaryTests, GivenBinaryWhenInvalidCURRENT_ICBE_VERSIONThenInvalidCURRENT_ICBE_VERSIONTIsReturned) {
    uint32_t binaryVersion;
    CLElfLib::ElfBinaryStorage elfBinary;

    CLElfLib::CElfWriter elfWriter(CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_EXECUTABLE, CLElfLib::E_EH_MACHINE::EH_MACHINE_NONE, 0);

    char *genBinary;
    size_t genBinarySize = sizeof(SProgramBinaryHeader);
    SProgramBinaryHeader genBinaryHeader = {0};
    genBinaryHeader.Magic = iOpenCL::MAGIC_CL;
    genBinaryHeader.Version = iOpenCL::CURRENT_ICBE_VERSION - 3u;
    genBinary = reinterpret_cast<char *>(&genBinaryHeader);

    if (genBinary) {
        std::string genBinaryTemp = genBinary ? std::string(genBinary, genBinarySize) : "";
        elfWriter.addSection(CLElfLib::SSectionNode(CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_DEV_BINARY, CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "Intel(R) OpenCL Device Binary", std::move(genBinaryTemp), static_cast<uint32_t>(genBinarySize)));
    }

    elfBinary.resize(elfWriter.getTotalBinarySize());
    elfWriter.resolveBinary(elfBinary);

    cl_int retVal = program->processElfBinary(elfBinary.data(), elfBinary.size(), binaryVersion);

    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    EXPECT_EQ(binaryVersion, iOpenCL::CURRENT_ICBE_VERSION - 3u);
}
