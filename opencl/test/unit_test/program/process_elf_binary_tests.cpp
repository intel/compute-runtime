/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/string.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

#include <cstring>

using namespace NEO;

class ProcessElfBinaryTests : public ::testing::Test {
  public:
    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, rootDeviceIndex));
        program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*device));
    }
    std::unique_ptr<ClDevice> device;
    std::unique_ptr<MockProgram> program;
    const uint32_t rootDeviceIndex = 1;
};

TEST_F(ProcessElfBinaryTests, GivenNullWhenCreatingProgramFromBinaryThenInvalidBinaryErrorIsReturned) {
    cl_int retVal = program->createProgramFromBinary(nullptr, 0, *device);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

TEST_F(ProcessElfBinaryTests, GivenInvalidBinaryWhenCreatingProgramFromBinaryThenExpectedErrorIsReturned) {
    char pBinary[] = "thisistotallyinvalid\0";
    size_t binarySize = strnlen_s(pBinary, 21);
    cl_int retVal = program->createProgramFromBinary(pBinary, binarySize, *device);

    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

TEST_F(ProcessElfBinaryTests, GivenValidBinaryWhenCreatingProgramFromBinaryThenSuccessIsReturned) {
    ZebinTestData::ValidEmptyProgram zebin;
    const auto &pBinary = zebin.storage;
    auto binarySize = zebin.storage.size();

    cl_int retVal = program->createProgramFromBinary(pBinary.data(), binarySize, *device);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary.data(), program->buildInfos[rootDeviceIndex].packedDeviceBinary.get(), binarySize));
}

TEST_F(ProcessElfBinaryTests, GivenValidSpirBinaryWhenCreatingProgramFromBinaryThenSuccessIsReturned) {
    // clCreateProgramWithIL => SPIR-V stored as source code
    const uint32_t spirvBinary[2] = {0x03022307, 0x07230203};
    size_t spirvBinarySize = sizeof(spirvBinary);

    // clCompileProgram => SPIR-V stored as IR binary
    program->isSpirV = true;
    program->irBinary = makeCopy(spirvBinary, spirvBinarySize);
    program->irBinarySize = spirvBinarySize;
    program->deviceBuildInfos[device.get()].programBinaryType = CL_PROGRAM_BINARY_TYPE_LIBRARY;
    EXPECT_NE(nullptr, program->irBinary);
    EXPECT_NE(0u, program->irBinarySize);
    EXPECT_TRUE(program->getIsSpirV());

    // clGetProgramInfo => SPIR-V stored as ELF binary
    cl_int retVal = program->packDeviceBinary(*device);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, program->buildInfos[rootDeviceIndex].packedDeviceBinary);
    EXPECT_NE(0u, program->buildInfos[rootDeviceIndex].packedDeviceBinarySize);

    // use ELF reader to parse and validate ELF binary
    std::string decodeErrors;
    std::string decodeWarnings;
    auto elf = NEO::Elf::decodeElf(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(program->buildInfos[rootDeviceIndex].packedDeviceBinary.get()), program->buildInfos[rootDeviceIndex].packedDeviceBinarySize), decodeErrors, decodeWarnings);
    auto header = elf.elfFileHeader;
    ASSERT_NE(nullptr, header);

    // check if ELF binary contains section SectionHeaderType_SPIRV
    bool hasSpirvSection = false;
    for (const auto &elfSectionHeader : elf.sectionHeaders) {
        if (elfSectionHeader.header->type == NEO::Elf::SHT_OPENCL_SPIRV) {
            hasSpirvSection = true;
            break;
        }
    }
    EXPECT_TRUE(hasSpirvSection);

    // clCreateProgramWithBinary => new program should recognize SPIR-V binary
    program->isSpirV = false;
    auto elfBinary = makeCopy(program->buildInfos[rootDeviceIndex].packedDeviceBinary.get(), program->buildInfos[rootDeviceIndex].packedDeviceBinarySize);
    retVal = program->createProgramFromBinary(elfBinary.get(), program->buildInfos[rootDeviceIndex].packedDeviceBinarySize, *device);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(program->getIsSpirV());
}

unsigned int binaryTypeValues[] = {
    CL_PROGRAM_BINARY_TYPE_EXECUTABLE,
    CL_PROGRAM_BINARY_TYPE_LIBRARY,
    CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT};

class ProcessElfBinaryTestsWithBinaryType : public ::testing::TestWithParam<unsigned int> {
  public:
    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, rootDeviceIndex));
        program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*device));
    }
    std::unique_ptr<ClDevice> device;
    std::unique_ptr<MockProgram> program;
    const uint32_t rootDeviceIndex = 1;
};

TEST_P(ProcessElfBinaryTestsWithBinaryType, GivenBinaryTypeWhenResolveProgramThenProgramIsProperlyResolved) {
    const uint8_t spirvData[] = {0x07, 0x23, 0x02, 0x03};

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject,
                             ArrayRef<const uint8_t>(spirvData, sizeof(spirvData)));
    auto inputBinary = elfEncoder.encode();

    cl_int retVal = program->createProgramFromBinary(inputBinary.data(), inputBinary.size(), *device);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto irBinary = makeCopy(program->irBinary.get(), program->irBinarySize);
    auto irBinarySize = program->irBinarySize;

    program->buildInfos[rootDeviceIndex].packedDeviceBinary.reset();
    program->buildInfos[rootDeviceIndex].packedDeviceBinarySize = 0U;
    program->deviceBuildInfos[device.get()].programBinaryType = GetParam();
    retVal = program->packDeviceBinary(*device);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, program->buildInfos[rootDeviceIndex].packedDeviceBinary);

    std::string decodeErrors;
    std::string decodeWarnings;
    auto elf = NEO::Elf::decodeElf(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(program->buildInfos[rootDeviceIndex].packedDeviceBinary.get()), program->buildInfos[rootDeviceIndex].packedDeviceBinarySize), decodeErrors, decodeWarnings);
    ASSERT_NE(nullptr, elf.elfFileHeader);

    if (GetParam() == CL_PROGRAM_BINARY_TYPE_LIBRARY) {
        EXPECT_EQ(NEO::Elf::ET_OPENCL_LIBRARY, elf.elfFileHeader->type);
    } else {
        EXPECT_EQ(NEO::Elf::ET_OPENCL_OBJECTS, elf.elfFileHeader->type);
    }

    ArrayRef<const uint8_t> decodedIr;
    for (auto &section : elf.sectionHeaders) {
        if (section.header->type == NEO::Elf::SHT_OPENCL_SPIRV) {
            decodedIr = section.data;
        }
    }
    ASSERT_EQ(irBinarySize, decodedIr.size());
    EXPECT_EQ(0, memcmp(irBinary.get(), decodedIr.begin(), irBinarySize));
}

INSTANTIATE_TEST_SUITE_P(ResolveBinaryTests,
                         ProcessElfBinaryTestsWithBinaryType,
                         ::testing::ValuesIn(binaryTypeValues));

TEST_F(ProcessElfBinaryTests, GivenEmptyBuildOptionsWhenCreatingProgramFromBinaryThenSuccessIsReturned) {
    const uint8_t spirvData[] = {0x07, 0x23, 0x02, 0x03};

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject,
                             ArrayRef<const uint8_t>(spirvData, sizeof(spirvData)));
    auto binary = elfEncoder.encode();

    cl_int retVal = program->createProgramFromBinary(binary.data(), binary.size(), *device);

    EXPECT_EQ(CL_SUCCESS, retVal);
    const auto &options = program->getOptions();
    size_t optionsSize = strlen(options.c_str()) + 1;
    EXPECT_EQ(0, memcmp("", options.c_str(), optionsSize));
}

TEST_F(ProcessElfBinaryTests, GivenNonEmptyBuildOptionsWhenCreatingProgramFromBinaryThenSuccessIsReturned) {
    auto buildOptionsNotEmpty = CompilerOptions::concatenate(CompilerOptions::optDisable, "-DDEF_WAS_SPECIFIED=1");
    const uint8_t spirvData[] = {0x07, 0x23, 0x02, 0x03};
    const auto optionsData = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildOptionsNotEmpty.c_str()),
                                                     buildOptionsNotEmpty.size());

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject,
                             ArrayRef<const uint8_t>(spirvData, sizeof(spirvData)));
    elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_OPTIONS, NEO::Elf::SectionNamesOpenCl::buildOptions, optionsData);
    auto binary = elfEncoder.encode();

    cl_int retVal = program->createProgramFromBinary(binary.data(), binary.size(), *device);

    EXPECT_EQ(CL_SUCCESS, retVal);
    const auto &options = program->getOptions();
    EXPECT_TRUE(hasSubstr(options, buildOptionsNotEmpty));
}
