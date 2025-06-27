/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/string.h"
#include "shared/test/common/device_binary_format/elf/elf_tests_data.h"
#include "shared/test/common/device_binary_format/patchtokens_tests.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_device.h"

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

TEST_F(ProcessElfBinaryTests, GivenInvalidBinaryWhenCreatingProgramFromBinaryThenInvalidBinaryErrorIsReturned) {
    char pBinary[] = "thisistotallyinvalid\0";
    size_t binarySize = strnlen_s(pBinary, 21);
    cl_int retVal = program->createProgramFromBinary(pBinary, binarySize, *device);

    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

TEST_F(ProcessElfBinaryTests, GivenValidBinaryWhenCreatingProgramFromBinaryThenSuccessIsReturned) {
    auto mockElf = std::make_unique<MockElfBinaryPatchtokens<>>(device->getHardwareInfo());
    auto pBinary = mockElf->storage;
    auto binarySize = mockElf->storage.size();

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
    auto mockElf = std::make_unique<MockElfBinaryPatchtokens<EnabledIrFormat::spirv>>(device->getHardwareInfo());
    auto pBinary = mockElf->storage;
    auto binarySize = mockElf->storage.size();

    cl_int retVal = program->createProgramFromBinary(pBinary.data(), binarySize, *device);
    auto options = program->options;
    auto genBinary = makeCopy(program->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get(), program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
    auto genBinarySize = program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize;
    auto irBinary = makeCopy(program->irBinary.get(), program->irBinarySize);
    auto irBinarySize = program->irBinarySize;

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(binarySize, program->buildInfos[rootDeviceIndex].packedDeviceBinarySize);
    EXPECT_EQ(0, memcmp(pBinary.data(), program->buildInfos[rootDeviceIndex].packedDeviceBinary.get(), binarySize));

    // delete program's elf reference to force a resolve
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

INSTANTIATE_TEST_SUITE_P(ResolveBinaryTests,
                         ProcessElfBinaryTestsWithBinaryType,
                         ::testing::ValuesIn(binaryTypeValues));

TEST_F(ProcessElfBinaryTests, GivenEmptyBuildOptionsWhenCreatingProgramFromBinaryThenSuccessIsReturned) {
    auto mockElf = std::make_unique<MockElfBinaryPatchtokens<>>(device->getHardwareInfo());
    auto pBinary = mockElf->storage;
    auto binarySize = mockElf->storage.size();

    cl_int retVal = program->createProgramFromBinary(pBinary.data(), binarySize, *device);

    EXPECT_EQ(CL_SUCCESS, retVal);
    const auto &options = program->getOptions();
    size_t optionsSize = strlen(options.c_str()) + 1;
    EXPECT_EQ(0, memcmp("", options.c_str(), optionsSize));
}

TEST_F(ProcessElfBinaryTests, GivenNonEmptyBuildOptionsWhenCreatingProgramFromBinaryThenSuccessIsReturned) {
    auto buildOptionsNotEmpty = CompilerOptions::concatenate(CompilerOptions::optDisable, "-DDEF_WAS_SPECIFIED=1");
    auto mockElf = std::make_unique<MockElfBinaryPatchtokens<>>(buildOptionsNotEmpty, device->getHardwareInfo());
    auto pBinary = mockElf->storage;
    auto binarySize = mockElf->storage.size();

    cl_int retVal = program->createProgramFromBinary(pBinary.data(), binarySize, *device);

    EXPECT_EQ(CL_SUCCESS, retVal);
    const auto &options = program->getOptions();
    EXPECT_TRUE(hasSubstr(options, buildOptionsNotEmpty));
}

TEST_F(ProcessElfBinaryTests, GivenBinaryWhenIncompatiblePatchtokenVerionThenProramCreationFails) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    {
        NEO::Elf::ElfEncoder<> elfEncoder;
        elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
        elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_DEV_BINARY, NEO::Elf::SectionNamesOpenCl::deviceBinary, programTokens.storage);
        auto elfBinary = elfEncoder.encode();
        cl_int retVal = program->createProgramFromBinary(elfBinary.data(), elfBinary.size(), *device);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    {
        programTokens.headerMutable->Version -= 1;
        NEO::Elf::ElfEncoder<> elfEncoder;
        elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
        elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_DEV_BINARY, NEO::Elf::SectionNamesOpenCl::deviceBinary, programTokens.storage);
        auto elfBinary = elfEncoder.encode();
        cl_int retVal = program->createProgramFromBinary(elfBinary.data(), elfBinary.size(), *device);
        EXPECT_EQ(CL_INVALID_BINARY, retVal);
    }
}
