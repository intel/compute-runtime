/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/file_io.h"
#include "runtime/program/program.h"
#include "runtime/helpers/string.h"
#include "unit_tests/helpers/test_files.h"
#include "gtest/gtest.h"
#include <cstring>

using namespace OCLRT;

class ProcessElfBinaryTests : public Program,
                              public ::testing::Test {
};

TEST_F(ProcessElfBinaryTests, NullBinary) {
    uint32_t binaryVersion;
    cl_int retVal = processElfBinary(nullptr, 0, binaryVersion);

    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    EXPECT_EQ(nullptr, elfBinary);
    EXPECT_EQ(0u, elfBinarySize);
    EXPECT_NE(0u, binaryVersion);
}

TEST_F(ProcessElfBinaryTests, InvalidBinary) {
    uint32_t binaryVersion;
    char pBinary[] = "thisistotallyinvalid\0";
    size_t binarySize = strnlen_s(pBinary, 21);
    cl_int retVal = processElfBinary(pBinary, binarySize, binaryVersion);

    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    EXPECT_EQ(nullptr, elfBinary);
    EXPECT_EQ(0u, elfBinarySize);
    EXPECT_NE(0u, binaryVersion);
}

TEST_F(ProcessElfBinaryTests, ValidBinary) {
    uint32_t binaryVersion;
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");

    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    cl_int retVal = processElfBinary(pBinary, binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary, elfBinary, binarySize));
    EXPECT_NE(0u, binaryVersion);
    deleteDataReadFromFile(pBinary);
}

TEST_F(ProcessElfBinaryTests, ValidSpirvBinary) {
    //clCreateProgramWithIL => SPIR-V stored as source code
    const uint32_t spirvBinary[2] = {0x03022307, 0x07230203};
    size_t spirvBinarySize = sizeof(spirvBinary);
    isSpirV = Program::isValidSpirvBinary(spirvBinary, spirvBinarySize);
    EXPECT_TRUE(isSpirV);

    //clCompileProgram => SPIR-V stored as IR binary
    storeIrBinary(spirvBinary, spirvBinarySize, true);
    programBinaryType = CL_PROGRAM_BINARY_TYPE_LIBRARY;
    EXPECT_NE(nullptr, irBinary);
    EXPECT_NE(0u, irBinarySize);
    EXPECT_TRUE(isSpirV);

    //clGetProgramInfo => SPIR-V stored as ELF binary
    cl_int retVal = resolveProgramBinary();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, elfBinary);
    EXPECT_NE(0u, elfBinarySize);

    //use ELF reader to parse and validate ELF binary
    CLElfLib::CElfReader *pElfReader = CLElfLib::CElfReader::create(
        reinterpret_cast<const char *>(elfBinary), elfBinarySize);
    ASSERT_NE(nullptr, pElfReader);
    const CLElfLib::SElf64Header *pElfHeader = pElfReader->getElfHeader();
    ASSERT_NE(nullptr, pElfHeader);
    EXPECT_EQ(pElfHeader->Type, CLElfLib::EH_TYPE_OPENCL_LIBRARY);
    EXPECT_TRUE(CLElfLib::CElfReader::isValidElf64(elfBinary, elfBinarySize));

    //check if ELF binary contains section SH_TYPE_SPIRV
    bool hasSpirvSection = false;
    for (uint32_t i = 1; i < pElfHeader->NumSectionHeaderEntries; i++) {
        const CLElfLib::SElf64SectionHeader *pSectionHeader = pElfReader->getSectionHeader(i);
        ASSERT_NE(nullptr, pSectionHeader);
        if (pSectionHeader->Type == CLElfLib::SH_TYPE_SPIRV) {
            hasSpirvSection = true;
            break;
        }
    }
    EXPECT_TRUE(hasSpirvSection);

    //clCreateProgramWithBinary => new program should recognize SPIR-V binary
    isSpirV = false;
    uint32_t elfBinaryVersion;
    auto pElfBinary = std::unique_ptr<char>(new char[elfBinarySize]);
    memcpy_s(pElfBinary.get(), elfBinarySize, elfBinary, elfBinarySize);
    retVal = processElfBinary(pElfBinary.get(), elfBinarySize, elfBinaryVersion);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(isSpirV);

    CLElfLib::CElfReader::destroy(pElfReader);
}

unsigned int BinaryTypeValues[] = {
    CL_PROGRAM_BINARY_TYPE_EXECUTABLE,
    CL_PROGRAM_BINARY_TYPE_LIBRARY,
    CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT};

class ProcessElfBinaryTestsWithBinaryType : public Program,
                                            public ::testing::TestWithParam<unsigned int> {
};

TEST_P(ProcessElfBinaryTestsWithBinaryType, GivenBinaryTypeWhenResolveProgramThenProgramIsProperlyResolved) {
    uint32_t binaryVersion;
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");

    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    cl_int retVal = processElfBinary(pBinary, binarySize, binaryVersion);

    size_t optionsSize = strlen(options.c_str()) + 1;
    auto pTmpGenBinary = new char[genBinarySize];
    auto pTmpIrBinary = new char[irBinarySize];
    auto pTmpOptions = new char[optionsSize];

    memcpy_s(pTmpGenBinary, genBinarySize, genBinary, genBinarySize);
    memcpy_s(pTmpIrBinary, irBinarySize, irBinary, irBinarySize);
    memcpy_s(pTmpOptions, optionsSize, options.c_str(), optionsSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary, elfBinary, binarySize));
    EXPECT_NE(0u, binaryVersion);

    // delete program's elf reference to force a resolve
    isProgramBinaryResolved = false;
    programBinaryType = GetParam();
    retVal = resolveProgramBinary();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pTmpGenBinary, genBinary, genBinarySize));
    EXPECT_EQ(0, memcmp(pTmpIrBinary, irBinary, irBinarySize));
    EXPECT_EQ(0, memcmp(pTmpOptions, options.c_str(), optionsSize));

    delete[] pTmpGenBinary;
    delete[] pTmpIrBinary;
    delete[] pTmpOptions;

    deleteDataReadFromFile(pBinary);
}

INSTANTIATE_TEST_CASE_P(ResolveBinaryTests,
                        ProcessElfBinaryTestsWithBinaryType,
                        ::testing::ValuesIn(BinaryTypeValues));

TEST_F(ProcessElfBinaryTests, BackToBack) {
    uint32_t binaryVersion;
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");

    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    cl_int retVal = processElfBinary(pBinary, binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary, elfBinary, binarySize));
    EXPECT_NE(0u, binaryVersion);
    deleteDataReadFromFile(pBinary);

    std::string filePath2;
    retrieveBinaryKernelFilename(filePath2, "simple_arg_int_", ".bin");

    binarySize = loadDataFromFile(filePath2.c_str(), pBinary);
    retVal = processElfBinary(pBinary, binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary, elfBinary, binarySize));
    EXPECT_NE(0u, binaryVersion);
    deleteDataReadFromFile(pBinary);
}

TEST_F(ProcessElfBinaryTests, BuildOptionsEmpty) {
    uint32_t binaryVersion;
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "simple_kernels_", ".bin");

    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    cl_int retVal = processElfBinary(pBinary, binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    size_t optionsSize = strlen(options.c_str()) + 1;
    EXPECT_EQ(0, memcmp("", options.c_str(), optionsSize));
    EXPECT_NE(0u, binaryVersion);
    deleteDataReadFromFile(pBinary);
}

TEST_F(ProcessElfBinaryTests, BuildOptionsNotEmpty) {
    uint32_t binaryVersion;
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "simple_kernels_opts_", ".bin");

    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    cl_int retVal = processElfBinary(pBinary, binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    size_t optionsSize = strlen(options.c_str()) + 1;
    std::string buildOptionsNotEmpty = "-cl-opt-disable -DDEF_WAS_SPECIFIED=1";
    EXPECT_EQ(0, memcmp(buildOptionsNotEmpty.c_str(), options.c_str(), optionsSize));
    EXPECT_NE(0u, binaryVersion);
    deleteDataReadFromFile(pBinary);
}
