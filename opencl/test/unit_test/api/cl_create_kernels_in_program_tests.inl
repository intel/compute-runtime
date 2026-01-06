/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/mocks/mock_kernel_info.h"
#include "shared/test/common/mocks/mock_zebin_wrapper.h"

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

struct ClCreateKernelsInProgramTests : public ApiTests {
    void SetUp() override {
        debugManager.flags.FailBuildProgramWithStatefulAccess.set(0);

        ApiTests::SetUp();

        constexpr auto numBits = is32bit ? Elf::EI_CLASS_32 : Elf::EI_CLASS_64;
        auto simd = std::max(16u, pDevice->getGfxCoreHelper().getMinimalSIMDSize());

        ZebinTestData::ZebinCopyBufferModule<numBits>::Descriptor desc{};
        desc.execEnv["simd_size"] = std::to_string(simd);
        auto zebinData = std::make_unique<ZebinTestData::ZebinCopyBufferModule<numBits>>(pDevice->getHardwareInfo(), desc);
        const auto &src = zebinData->storage;
        const auto &binarySize = src.size();

        ASSERT_NE(0u, src.size());
        ASSERT_NE(nullptr, src.data());

        auto binaryStatus = CL_SUCCESS;
        const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(src.data())};
        program = clCreateProgramWithBinary(
            pContext,
            1,
            &testedClDevice,
            &binarySize,
            binaries,
            &binaryStatus,
            &retVal);

        ASSERT_NE(nullptr, program);
        ASSERT_EQ(CL_SUCCESS, retVal);

        retVal = clBuildProgram(
            program,
            1,
            &testedClDevice,
            nullptr,
            nullptr,
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        ApiTests::TearDown();
    }

    cl_program program = nullptr;
    cl_kernel kernel = nullptr;
    std::unique_ptr<char[]> pBinary = nullptr;
    DebugManagerStateRestore restore;
};

TEST_F(ClCreateKernelsInProgramTests, GivenValidParametersWhenCreatingKernelObjectsThenKernelsAndSuccessAreReturned) {
    cl_uint numKernelsRet = 0;
    retVal = clCreateKernelsInProgram(
        program,
        1,
        &kernel,
        &numKernelsRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numKernelsRet);
    EXPECT_NE(nullptr, kernel);
}

TEST_F(ClCreateKernelsInProgramTests, GivenNullKernelArgWhenCreatingKernelObjectsThenSuccessIsReturned) {
    cl_uint numKernelsRet = 0;
    retVal = clCreateKernelsInProgram(
        program,
        0,
        nullptr,
        &numKernelsRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numKernelsRet);
}

TEST_F(ClCreateKernelsInProgramTests, GivenNullPtrForNumKernelsReturnWhenCreatingKernelObjectsThenSuccessIsReturned) {
    retVal = clCreateKernelsInProgram(
        program,
        1,
        &kernel,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, kernel);
}

TEST_F(ClCreateKernelsInProgramTests, GivenNullProgramWhenCreatingKernelObjectsThenInvalidProgramErrorIsReturn) {
    retVal = clCreateKernelsInProgram(
        nullptr,
        1,
        &kernel,
        nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
    EXPECT_EQ(nullptr, kernel);
}

TEST_F(ClCreateKernelsInProgramTests, GivenTooSmallOutputBufferWhenCreatingKernelObjectsThenInvalidValueErrorIsReturned) {
    retVal = clCreateKernelsInProgram(
        program,
        0,
        &kernel,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, kernel);
}

TEST_F(ClCreateKernelsInProgramTests, whenKernelCreationFailsOnClCreateKernelsInProgramAPICallThenPropagateTheErrorToAPICallAndReturnNullptrForEachKernel) {
    const auto rootDeviceIndex = pDevice->getRootDeviceIndex();
    auto &kernelInfoArray = pProgram->getKernelInfoArray(rootDeviceIndex);
    kernelInfoArray.push_back(new MockKernelInfo());
    kernelInfoArray.push_back(new MockKernelInfo());
    auto kernelInfo1 = kernelInfoArray[0];
    auto kernelInfo2 = kernelInfoArray[1];

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();

    // Enforce CL_INVALID_KERNEL error for kernel created using kernelInfo2
    // Kernel creation using kernelInfo1 should succeed.
    auto minimalSimdSize = gfxCoreHelper.getMinimalSIMDSize();
    kernelInfo1->kernelDescriptor.kernelAttributes.simdSize = minimalSimdSize;
    kernelInfo2->kernelDescriptor.kernelAttributes.simdSize = minimalSimdSize - 1;

    cl_kernel kernels[2];
    cl_uint numKernels = 2;
    retVal = clCreateKernelsInProgram(
        pProgram,
        numKernels,
        kernels,
        nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
    EXPECT_EQ(nullptr, kernels[0]);
    EXPECT_EQ(nullptr, kernels[1]);
}

TEST(ClCreateKernelsInProgramTest, givenMultiDeviceProgramWhenNotBuiltThenDevicesInProgramMatchContextDevices) {
    MockUnrestrictiveContextMultiGPU context;
    cl_program pProgram = nullptr;
    cl_int retVal = CL_INVALID_PROGRAM;
    MockZebinWrapper zebin{context.getDevice(0)->getHardwareInfo()};
    zebin.setAsMockCompilerReturnedBinary();

    const char *sourceKernel = "example_kernel(){}";
    size_t sourceKernelSize = std::strlen(sourceKernel) + 1;
    const char *sources[1] = {sourceKernel};

    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceKernelSize,
        &retVal);

    ASSERT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_uint numKernels = 0;
    retVal = clCreateKernelsInProgram(pProgram, 0, nullptr, &numKernels);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(numKernels, 0u);

    Program *program = castToObject<Program>(pProgram);
    EXPECT_EQ(context.getDevices().size(), program->getDevices().size());
    EXPECT_EQ(context.getDevices().size(), program->getDevicesInProgram().size());

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

struct DeviceIndices {
    std::vector<int> indices;
};

class ClCreateKernelsInProgramMultiDeviceTests : public ::testing::TestWithParam<DeviceIndices> {};

TEST_P(ClCreateKernelsInProgramMultiDeviceTests, givenMultiDeviceProgramWhenBuiltForVariousDeviceCombinationsThenProgramAndKernelDevicesMatch) {
    MockUnrestrictiveContextMultiGPU context;
    cl_program pProgram = nullptr;
    cl_int retVal = CL_INVALID_PROGRAM;
    MockZebinWrapper zebin{context.getDevice(0)->getHardwareInfo()};
    zebin.setAsMockCompilerReturnedBinary();

    const char *sourceKernel = "example_kernel(){}";
    size_t sourceKernelSize = std::strlen(sourceKernel) + 1;
    const char *sources[1] = {sourceKernel};

    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceKernelSize,
        &retVal);

    ASSERT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    std::vector<cl_device_id> devices;
    for (int idx : GetParam().indices) {
        if (idx == 0) {
            devices.push_back(context.pRootDevice0);
        } else if (idx == 1) {
            devices.push_back(context.pRootDevice1);
        }
    }

    retVal = clBuildProgram(
        pProgram,
        static_cast<cl_uint>(devices.size()),
        devices.data(),
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint numKernels = 0;
    retVal = clCreateKernelsInProgram(pProgram, 0, nullptr, &numKernels);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(numKernels, 1u);

    std::vector<cl_kernel> kernels(numKernels);
    retVal = clCreateKernelsInProgram(pProgram, numKernels, kernels.data(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (cl_kernel kernel : kernels) {
        EXPECT_NE(nullptr, kernel);
    }

    MultiDeviceKernel *multiDeviceKernel = castToObject<MultiDeviceKernel>(kernels[0]);
    Program *program = castToObject<Program>(pProgram);

    EXPECT_EQ(context.getDevices().size(), program->getDevices().size());
    EXPECT_EQ(multiDeviceKernel->getDevices(), program->getDevicesInProgram());

    for (cl_kernel kernel : kernels) {
        retVal = clReleaseKernel(kernel);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

INSTANTIATE_TEST_SUITE_P(
    DeviceCombinations,
    ClCreateKernelsInProgramMultiDeviceTests,
    ::testing::Values(
        DeviceIndices{{0}},   // firstDevice
        DeviceIndices{{1}},   // secondDevice
        DeviceIndices{{0, 1}} // deviceListBoth
        ));