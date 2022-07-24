/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/mocks/mock_compilers.h"

#include "opencl/source/context/context.h"
#include "opencl/source/program/program.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clBuildProgramTests;

namespace ULT {

TEST_F(clBuildProgramTests, GivenSourceAsInputWhenCreatingProgramWithSourceThenProgramBuildSucceeds) {
    cl_program pProgram = nullptr;
    std::unique_ptr<char[]> pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");

    pSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    const char *sourceArray[1] = {pSource.get()};

    pProgram = clCreateProgramWithSource(
        pContext,
        1,
        sourceArray,
        &sourceSize,
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
        sourceArray,
        &sourceSize,
        nullptr);
    EXPECT_EQ(nullptr, pProgram);
}

TEST_F(clBuildProgramTests, GivenBinaryAsInputWhenCreatingProgramWithSourceThenProgramBuildSucceeds) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    std::unique_ptr<char[]> pBinary = nullptr;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    pBinary = loadDataFromFile(
        testFile.c_str(),
        binarySize);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);

    const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(pBinary.get())};
    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        &binarySize,
        binaries,
        &binaryStatus,
        &retVal);

    pBinary.reset();

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

TEST_F(clBuildProgramTests, GivenBinaryAsInputWhenCreatingProgramWithBinaryForMultipleDevicesThenProgramBuildSucceeds) {
    MockUnrestrictiveContextMultiGPU context;
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    std::unique_ptr<char[]> pBinary = nullptr;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    pBinary = loadDataFromFile(
        testFile.c_str(),
        binarySize);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);

    const size_t numBinaries = 6;
    const unsigned char *binaries[numBinaries];
    std::fill(binaries, binaries + numBinaries, reinterpret_cast<const unsigned char *>(pBinary.get()));
    cl_device_id devicesForProgram[] = {context.pRootDevice0, context.pSubDevice00, context.pSubDevice01, context.pRootDevice1, context.pSubDevice10, context.pSubDevice11};
    size_t sizeBinaries[numBinaries];
    std::fill(sizeBinaries, sizeBinaries + numBinaries, binarySize);

    pProgram = clCreateProgramWithBinary(
        &context,
        numBinaries,
        devicesForProgram,
        sizeBinaries,
        binaries,
        &binaryStatus,
        &retVal);

    pBinary.reset();

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

TEST_F(clBuildProgramTests, GivenProgramCreatedFromBinaryWhenBuildProgramWithOptionsIsCalledThenStoredOptionsAreUsed) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    auto pBinary = loadDataFromFile(
        testFile.c_str(),
        binarySize);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);
    const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(pBinary.get())};
    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        &binarySize,
        binaries,
        &binaryStatus,
        &retVal);

    auto pInternalProgram = castToObject<Program>(pProgram);

    pBinary.reset();
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

TEST_F(clBuildProgramTests, GivenSpirAsInputWhenCreatingProgramFromBinaryThenProgramBuildSucceeds) {
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

TEST_F(clBuildProgramTests, GivenNullAsInputWhenCreatingProgramThenInvalidProgramErrorIsReturned) {
    retVal = clBuildProgram(
        nullptr,
        1,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
}

TEST_F(clBuildProgramTests, GivenInvalidCallbackInputWhenBuildProgramThenInvalidValueErrorIsReturned) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    auto pBinary = loadDataFromFile(
        testFile.c_str(),
        binarySize);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);
    const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(pBinary.get())};
    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        &binarySize,
        binaries,
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

TEST_F(clBuildProgramTests, GivenValidCallbackInputWhenBuildProgramThenCallbackIsInvoked) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    auto pBinary = loadDataFromFile(
        testFile.c_str(),
        binarySize);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);
    const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(pBinary.get())};
    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        &binarySize,
        binaries,
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

TEST_F(clBuildProgramTests, givenProgramWhenBuildingForInvalidDevicesInputThenInvalidDeviceErrorIsReturned) {
    cl_program pProgram = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    testFile.append(clFiles);
    testFile.append("copybuffer.cl");
    auto pSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        pContext,
        1,
        sources,
        &sourceSize,
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

TEST(clBuildProgramTest, givenMultiDeviceProgramWithCreatedKernelWhenBuildingThenInvalidOperationErrorIsReturned) {

    MockSpecializedContext context;
    cl_program pProgram = nullptr;
    size_t sourceSize = 0;
    cl_int retVal = CL_INVALID_PROGRAM;
    std::string testFile;

    testFile.append(clFiles);
    testFile.append("copybuffer.cl");
    auto pSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceSize,
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

    auto kernel = clCreateKernel(pProgram, "fullCopy", &retVal);

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

TEST(clBuildProgramTest, givenMultiDeviceProgramWithCreatedKernelsWhenBuildingThenInvalidOperationErrorIsReturned) {

    MockSpecializedContext context;
    cl_program pProgram = nullptr;
    size_t sourceSize = 0;
    cl_int retVal = CL_INVALID_PROGRAM;
    std::string testFile;

    testFile.append(clFiles);
    testFile.append("copybuffer.cl");
    auto pSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceSize,
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

TEST(clBuildProgramTest, givenMultiDeviceProgramWithProgramBuiltForSingleDeviceWhenCreatingKernelThenProgramAndKernelDevicesMatchAndSuccessIsReturned) {
    MockUnrestrictiveContextMultiGPU context;
    cl_program pProgram = nullptr;
    size_t sourceSize = 0;
    cl_int retVal = CL_INVALID_PROGRAM;
    std::string testFile;

    testFile.append(clFiles);
    testFile.append("copybuffer.cl");
    auto pSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceSize,
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

    cl_kernel pKernel = clCreateKernel(pProgram, "fullCopy", &retVal);
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

TEST(clBuildProgramTest, givenMultiDeviceProgramWithProgramBuiltForSingleDeviceWithCreatedKernelWhenBuildingProgramForSecondDeviceThenInvalidOperationReturned) {
    MockUnrestrictiveContextMultiGPU context;
    cl_program pProgram = nullptr;
    size_t sourceSize = 0;
    cl_int retVal = CL_INVALID_PROGRAM;
    std::string testFile;

    testFile.append(clFiles);
    testFile.append("copybuffer.cl");
    auto pSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceSize,
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

    cl_kernel kernel = clCreateKernel(pProgram, "fullCopy", &retVal);
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

    kernel = clCreateKernel(pProgram, "fullCopy", &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clBuildProgramTest, givenMultiDeviceProgramWithProgramBuiltForMultipleDevicesSeparatelyWithCreatedKernelThenProgramAndKernelDevicesMatch) {
    MockUnrestrictiveContextMultiGPU context;
    cl_program pProgram = nullptr;
    size_t sourceSize = 0;
    cl_int retVal = CL_INVALID_PROGRAM;
    std::string testFile;

    testFile.append(clFiles);
    testFile.append("copybuffer.cl");
    auto pSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceSize,
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

    cl_kernel pKernel = clCreateKernel(pProgram, "fullCopy", &retVal);
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
