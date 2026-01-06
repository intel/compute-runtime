/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_zebin_wrapper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/context/context.h"
#include "opencl/source/program/program.h"

#include "cl_api_tests.h"

using namespace NEO;

struct ClBuildProgramTests : public ApiTests {
    void SetUp() override {
        debugManager.flags.FailBuildProgramWithStatefulAccess.set(0);
        debugManager.flags.EnableAIL.set(false);
        ApiTests::setUp();
    }
    void TearDown() override {
        ApiTests::tearDown();
    }

    DebugManagerStateRestore restore;
};

namespace ULT {

TEST_F(ClBuildProgramTests, GivenSourceAsInputWhenCreatingProgramWithSourceThenProgramBuildSucceeds) {
    cl_program pProgram = nullptr;
    MockZebinWrapper zebin{pDevice->getHardwareInfo()};
    zebin.setAsMockCompilerReturnedBinary();

    pProgram = clCreateProgramWithSource(
        pContext,
        1,
        sampleKernelSrcs,
        &sampleKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        1,
        &testedClDevice,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pProgram = clCreateProgramWithSource(
        nullptr,
        1,
        sampleKernelSrcs,
        &sampleKernelSize,
        nullptr);
    EXPECT_EQ(nullptr, pProgram);
}

TEST_F(ClBuildProgramTests, GivenBinaryAsInputWhenCreatingProgramWithSourceThenProgramBuildSucceeds) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    MockZebinWrapper zebin{pDevice->getHardwareInfo()};

    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        1,
        &testedClDevice,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST2_F(ClBuildProgramTests, GivenFailBuildProgramAndBinaryAsInputWhenCreatingProgramWithSourceThenProgramBuildFails, IsAtLeastXeHpcCore) {
    debugManager.flags.FailBuildProgramWithStatefulAccess.set(1);
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    MockZebinWrapper zebin{pDevice->getHardwareInfo()};

    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        1,
        &testedClDevice,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST2_F(ClBuildProgramTests, GivenFailBuildProgramAndBinaryGeneratedByNgenAsInputWhenCreatingProgramWithSourceThenProgramBuildReturnsSuccess, IsAtLeastXeHpcCore) {
    debugManager.flags.FailBuildProgramWithStatefulAccess.set(1);
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    MockZebinWrapper zebin{pDevice->getHardwareInfo()};

    zebin.getFlags().generatorId = 0u; // ngen generated

    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        1,
        &testedClDevice,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClBuildProgramTests, GivenBinaryAsInputWhenCreatingProgramWithBinaryForMultipleDevicesThenProgramBuildSucceeds) {
    MockUnrestrictiveContextMultiGPU context;
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    const size_t numBinaries = 6;
    MockZebinWrapper<6u> zebin{pDevice->getHardwareInfo()};

    cl_device_id devicesForProgram[] = {context.pRootDevice0, context.pSubDevice00, context.pSubDevice01, context.pRootDevice1, context.pSubDevice10, context.pSubDevice11};

    pProgram = clCreateProgramWithBinary(
        &context,
        numBinaries,
        devicesForProgram,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClBuildProgramTests, GivenProgramCreatedFromBinaryWhenBuildProgramWithOptionsIsCalledThenStoredOptionsAreUsed) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    MockZebinWrapper zebin{pDevice->getHardwareInfo()};

    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        &retVal);

    auto pInternalProgram = castToObject<Program>(pProgram);

    auto storedOptionsSize = pInternalProgram->getOptions().size();

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    const char *newBuildOption = "cl-fast-relaxed-math";

    retVal = clBuildProgram(
        pProgram,
        1,
        &testedClDevice,
        newBuildOption,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    auto optionsAfterBuildSize = pInternalProgram->getOptions().size();

    EXPECT_EQ(optionsAfterBuildSize, storedOptionsSize);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClBuildProgramTests, GivenSpirAsInputWhenCreatingProgramFromBinaryThenProgramBuildSucceeds) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    unsigned char llvm[16] = "BC\xc0\xde_unique";
    size_t binarySize = sizeof(llvm);

    const unsigned char *binaries[1] = {llvm};
    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        &binarySize,
        binaries,
        &binaryStatus,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    MockCompilerDebugVars igcDebugVars;
    SProgramBinaryHeader progBin = {};
    progBin.Magic = iOpenCL::MAGIC_CL;
    progBin.Version = iOpenCL::CURRENT_ICBE_VERSION;
    progBin.Device = pContext->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily;
    progBin.GPUPointerSizeInBytes = sizeof(uintptr_t);
    igcDebugVars.binaryToReturn = &progBin;
    igcDebugVars.binaryToReturnSize = sizeof(progBin);
    auto prevDebugVars = getIgcDebugVars();
    setIgcDebugVars(igcDebugVars);
    retVal = clBuildProgram(
        pProgram,
        1,
        &testedClDevice,
        "-x spir -spir-std=1.2",
        nullptr,
        nullptr);
    setIgcDebugVars(prevDebugVars);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClBuildProgramTests, GivenNullAsInputWhenCreatingProgramThenInvalidProgramErrorIsReturned) {
    retVal = clBuildProgram(
        nullptr,
        1,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
}

TEST_F(ClBuildProgramTests, GivenInvalidCallbackInputWhenBuildProgramThenInvalidValueErrorIsReturned) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    MockZebinWrapper zebin{pDevice->getHardwareInfo()};

    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pProgram);

    retVal = clBuildProgram(
        pProgram,
        1,
        &testedClDevice,
        nullptr,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClBuildProgramTests, GivenValidCallbackInputWhenBuildProgramThenCallbackIsInvoked) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    MockZebinWrapper zebin{pDevice->getHardwareInfo()};

    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pProgram);

    char userData = 0;

    retVal = clBuildProgram(
        pProgram,
        1,
        &testedClDevice,
        nullptr,
        notifyFuncProgram,
        &userData);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ('a', userData);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClBuildProgramTests, givenProgramWhenBuildingForInvalidDevicesInputThenInvalidDeviceErrorIsReturned) {
    cl_program pProgram = nullptr;
    MockZebinWrapper zebin{pDevice->getHardwareInfo()};
    zebin.setAsMockCompilerReturnedBinary();

    pProgram = clCreateProgramWithSource(
        pContext,
        1,
        sampleKernelSrcs,
        &sampleKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    MockContext mockContext;
    cl_device_id nullDeviceInput[] = {pContext->getDevice(0), nullptr};
    cl_device_id notAssociatedDeviceInput[] = {mockContext.getDevice(0)};
    cl_device_id validDeviceInput[] = {pContext->getDevice(0)};

    retVal = clBuildProgram(
        pProgram,
        0,
        validDeviceInput,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clBuildProgram(
        pProgram,
        1,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clBuildProgram(
        pProgram,
        2,
        nullDeviceInput,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);

    retVal = clBuildProgram(
        pProgram,
        1,
        notAssociatedDeviceInput,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

using ClBuildProgramMultiDeviceTests = Test<FixtureWithMockZebin>;

TEST_F(ClBuildProgramMultiDeviceTests, givenMultiDeviceProgramWithCreatedKernelWhenBuildingThenInvalidOperationErrorIsReturned) {
    MockSpecializedContext context;
    cl_program pProgram = nullptr;
    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_device_id firstSubDevice = context.pSubDevice0;
    cl_device_id secondSubDevice = context.pSubDevice1;

    retVal = clBuildProgram(
        pProgram,
        1,
        &firstSubDevice,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto kernel = clCreateKernel(pProgram, zebinPtr->kernelName, &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        1,
        &secondSubDevice,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        1,
        &secondSubDevice,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClBuildProgramMultiDeviceTests, givenMultiDeviceProgramWithCreatedKernelsWhenBuildingThenInvalidOperationErrorIsReturned) {
    MockSpecializedContext context;
    cl_program pProgram = nullptr;
    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_device_id firstSubDevice = context.pSubDevice0;
    cl_device_id secondSubDevice = context.pSubDevice1;

    retVal = clBuildProgram(
        pProgram,
        1,
        &firstSubDevice,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t numKernels = 0;
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_NUM_KERNELS, sizeof(numKernels), &numKernels, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto kernels = std::make_unique<cl_kernel[]>(numKernels);

    retVal = clCreateKernelsInProgram(pProgram, static_cast<cl_uint>(numKernels), kernels.get(), nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        1,
        &secondSubDevice,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    for (auto i = 0u; i < numKernels; i++) {
        retVal = clReleaseKernel(kernels[i]);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    retVal = clBuildProgram(
        pProgram,
        1,
        &secondSubDevice,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClBuildProgramMultiDeviceTests, givenMultiDeviceProgramWithProgramBuiltForSingleDeviceWhenCreatingKernelThenProgramAndKernelDevicesMatchAndSuccessIsReturned) {
    MockUnrestrictiveContextMultiGPU context;
    cl_program pProgram = nullptr;

    cl_int retVal = CL_INVALID_PROGRAM;
    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_device_id firstDevice = context.pRootDevice0;
    cl_device_id firstSubDevice = context.pSubDevice00;
    cl_device_id secondSubDevice = context.pSubDevice01;

    retVal = clBuildProgram(
        pProgram,
        1,
        &firstDevice,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_kernel pKernel = clCreateKernel(pProgram, zebinPtr->kernelName, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MultiDeviceKernel *kernel = castToObject<MultiDeviceKernel>(pKernel);
    auto devs = kernel->getDevices();
    EXPECT_EQ(devs[0], firstDevice);
    EXPECT_EQ(devs[1], firstSubDevice);
    EXPECT_EQ(devs[2], secondSubDevice);

    retVal = clReleaseKernel(pKernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClBuildProgramMultiDeviceTests, GivenProgramCreatedFromSourceWhenBuildingThenCorrectlyFilledSingleDeviceBinaryIsUsed) {
    MockUnrestrictiveContextMultiGPU context;
    cl_program pProgram = nullptr;

    std::string zeinfo = std::string("version :\'") + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
kernels:
    - name : some_kernel
      execution_env :
        simd_size : 32
        require_iab:     true
    - name : some_other_kernel
      execution_env :
        simd_size : 32
)===";

    uint8_t kernelIsa[8]{0U};
    ZebinTestData::ValidEmptyProgram<is32bit ? NEO::Elf::EI_CLASS_32 : NEO::Elf::EI_CLASS_64> zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeinfo.data(), zeinfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "some_kernel", {kernelIsa, sizeof(kernelIsa)});
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "some_other_kernel", {kernelIsa, sizeof(kernelIsa)});

    const uint8_t data[] = {'H', 'e', 'l', 'l', 'o', '!'};
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::dataConstString, data);

    {
        const uint32_t indirectAccessBufferMajorVersion = 4u;

        Zebin::Elf::ElfNoteSection elfNoteSection = {};
        elfNoteSection.type = Zebin::Elf::IntelGTSectionType::indirectAccessBufferMajorVersion;
        elfNoteSection.descSize = sizeof(uint32_t);
        elfNoteSection.nameSize = 8u;

        auto sectionDataSize = sizeof(Zebin::Elf::ElfNoteSection) + elfNoteSection.nameSize + alignUp(elfNoteSection.descSize, 4);
        auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
        auto appendSingleIntelGTSectionData = [](const NEO::Elf::ElfNoteSection &elfNoteSection, uint8_t *const intelGTSectionData, const uint8_t *descData, const char *ownerName, size_t spaceAvailable) {
            size_t offset = 0;
            ASSERT_GE(spaceAvailable, sizeof(Zebin::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize);
            memcpy_s(ptrOffset(intelGTSectionData, offset), sizeof(NEO::Elf::ElfNoteSection), &elfNoteSection, sizeof(NEO::Elf::ElfNoteSection));
            offset += sizeof(NEO::Elf::ElfNoteSection);
            memcpy_s(reinterpret_cast<char *>(ptrOffset(intelGTSectionData, offset)), elfNoteSection.nameSize, ownerName, elfNoteSection.nameSize);
            offset += elfNoteSection.nameSize;
            memcpy_s(ptrOffset(intelGTSectionData, offset), elfNoteSection.descSize, descData, elfNoteSection.descSize);
            offset += elfNoteSection.descSize;
        };

        appendSingleIntelGTSectionData(elfNoteSection, noteIntelGTSectionData.get(), reinterpret_cast<const uint8_t *>(&indirectAccessBufferMajorVersion),
                                       Zebin::Elf::intelGTNoteOwnerName.str().c_str(), sectionDataSize);
        zebin.appendSection(Zebin::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));
    }

    MockCompilerDebugVars debugVars;
    debugVars.binaryToReturn = const_cast<unsigned char *>(zebin.storage.data());
    debugVars.binaryToReturnSize = zebin.storage.size();
    gEnvironment->igcPushDebugVars(debugVars);
    gEnvironment->fclPushDebugVars(debugVars);

    cl_int retVal = CL_INVALID_PROGRAM;
    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_device_id firstDevice = context.pRootDevice0;
    cl_device_id secondDevice = context.pRootDevice1;
    cl_device_id devices[] = {firstDevice, secondDevice};

    retVal = clBuildProgram(
        pProgram,
        2,
        devices,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_kernel pKernel = clCreateKernel(pProgram, "some_kernel", &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MultiDeviceKernel *kernel = castToObject<MultiDeviceKernel>(pKernel);
    Program *program = castToObject<Program>(pProgram);
    EXPECT_EQ(4u, program->getIndirectAccessBufferVersion());
    EXPECT_TRUE(kernel->getKernelInfos()[1]->kernelDescriptor.kernelMetadata.isGeneratedByIgc);

    retVal = clReleaseKernel(pKernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    gEnvironment->igcPopDebugVars();
    gEnvironment->fclPopDebugVars();
}

TEST_F(ClBuildProgramMultiDeviceTests, givenMultiDeviceProgramWithProgramBuiltForSingleDeviceWithCreatedKernelWhenBuildingProgramForSecondDeviceThenInvalidOperationReturned) {
    MockUnrestrictiveContextMultiGPU context;
    cl_program pProgram = nullptr;
    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_device_id firstDevice = context.pRootDevice0;
    cl_device_id secondDevice = context.pRootDevice1;

    retVal = clBuildProgram(
        pProgram,
        1,
        &firstDevice,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_kernel kernel = clCreateKernel(pProgram, zebinPtr->kernelName, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        1,
        &secondDevice,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        1,
        &secondDevice,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    kernel = clCreateKernel(pProgram, zebinPtr->kernelName, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClBuildProgramMultiDeviceTests, givenMultiDeviceProgramWithProgramBuiltForMultipleDevicesSeparatelyWithCreatedKernelThenProgramAndKernelDevicesMatch) {
    MockUnrestrictiveContextMultiGPU context;
    cl_program pProgram = nullptr;
    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_device_id firstDevice = context.pRootDevice0;
    cl_device_id secondDevice = context.pRootDevice1;

    retVal = clBuildProgram(
        pProgram,
        1,
        &firstDevice,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        1,
        &secondDevice,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_kernel pKernel = clCreateKernel(pProgram, zebinPtr->kernelName, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MultiDeviceKernel *kernel = castToObject<MultiDeviceKernel>(pKernel);
    Program *program = castToObject<Program>(pProgram);
    EXPECT_EQ(kernel->getDevices(), program->getDevices());

    retVal = clReleaseKernel(pKernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
