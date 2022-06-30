/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/device/device.h"

#include "opencl/source/built_ins/built_in_ops_vme.h"
#include "opencl/source/built_ins/vme_builtin.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/fixtures/run_kernel_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clCreateProgramWithBuiltInKernelsTests;

struct clCreateProgramWithBuiltInVmeKernelsTests : clCreateProgramWithBuiltInKernelsTests {
    void SetUp() override {
        clCreateProgramWithBuiltInKernelsTests::SetUp();
        if (!castToObject<ClDevice>(testedClDevice)->getHardwareInfo().capabilityTable.supportsVme) {
            GTEST_SKIP();
        }

        pClDevice = pContext->getDevice(0);
    }

    ClDevice *pClDevice;
};

namespace ULT {

TEST_F(clCreateProgramWithBuiltInKernelsTests, GivenInvalidContextWhenCreatingProgramWithBuiltInKernelsThenInvalidContextErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto program = clCreateProgramWithBuiltInKernels(
        nullptr, // context
        1,       // num_devices
        nullptr, // device_list
        nullptr, // kernel_names
        &retVal);
    EXPECT_EQ(nullptr, program);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clCreateProgramWithBuiltInKernelsTests, GivenNoKernelsWhenCreatingProgramWithBuiltInKernelsThenInvalidValueErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto program = clCreateProgramWithBuiltInKernels(
        pContext,        // context
        1,               // num_devices
        &testedClDevice, // device_list
        "",              // kernel_names
        &retVal);
    EXPECT_EQ(nullptr, program);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateProgramWithBuiltInKernelsTests, GivenNoDeviceWhenCreatingProgramWithBuiltInKernelsThenInvalidValueErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto program = clCreateProgramWithBuiltInKernels(
        pContext,        // context
        0,               // num_devices
        &testedClDevice, // device_list
        "",              // kernel_names
        &retVal);
    EXPECT_EQ(nullptr, program);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateProgramWithBuiltInKernelsTests, GivenNoKernelsAndNoReturnWhenCreatingProgramWithBuiltInKernelsThenProgramIsNotCreated) {
    auto program = clCreateProgramWithBuiltInKernels(
        pContext,        // context
        1,               // num_devices
        &testedClDevice, // device_list
        "",              // kernel_names
        nullptr);
    EXPECT_EQ(nullptr, program);
}

TEST_F(clCreateProgramWithBuiltInVmeKernelsTests, GivenDeviceNotAssociatedWithContextWhenCreatingProgramWithBuiltInThenInvalidDeviceErrorIsReturned) {
    cl_program pProgram = nullptr;

    const char *kernelNamesString = {
        "block_advanced_motion_estimate_bidirectional_check_intel;"
        "block_motion_estimate_intel;"
        "block_advanced_motion_estimate_check_intel;"};

    MockClDevice invalidDevice(new MockDevice());

    cl_device_id devicesForProgram[] = {&invalidDevice};

    pProgram = clCreateProgramWithBuiltInKernels(
        pContext,
        1,
        devicesForProgram,
        kernelNamesString,
        &retVal);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
    EXPECT_EQ(nullptr, pProgram);

    retVal = CL_INVALID_PROGRAM;
    devicesForProgram[0] = nullptr;

    pProgram = clCreateProgramWithBuiltInKernels(
        pContext,
        1,
        devicesForProgram,
        kernelNamesString,
        &retVal);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
    EXPECT_EQ(nullptr, pProgram);
}

TEST_F(clCreateProgramWithBuiltInVmeKernelsTests, GivenValidMediaKernelsWhenCreatingProgramWithBuiltInKernelsThenProgramIsSuccessfullyCreated) {
    cl_int retVal = CL_SUCCESS;

    overwriteBuiltInBinaryName("media_kernels_frontend");

    const char *kernelNamesString = {
        "block_advanced_motion_estimate_bidirectional_check_intel;"
        "block_motion_estimate_intel;"
        "block_advanced_motion_estimate_check_intel;"};

    const char *kernelNames[] = {
        "block_motion_estimate_intel",
        "block_advanced_motion_estimate_check_intel",
        "block_advanced_motion_estimate_bidirectional_check_intel",
    };

    cl_program program = clCreateProgramWithBuiltInKernels(
        pContext,          // context
        1,                 // num_devices
        &testedClDevice,   // device_list
        kernelNamesString, // kernel_names
        &retVal);

    restoreBuiltInBinaryName();
    EXPECT_NE(nullptr, program);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &kernelName : kernelNames) {
        cl_kernel kernel = clCreateKernel(
            program,
            kernelName,
            &retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, kernel);

        retVal = clReleaseKernel(kernel);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    retVal = clReleaseProgram(program);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateProgramWithBuiltInVmeKernelsTests, GivenValidMediaKernelsWithOptionsWhenCreatingProgramWithBuiltInKernelsThenProgramIsSuccessfullyCreatedWithThoseOptions) {
    cl_int retVal = CL_SUCCESS;

    overwriteBuiltInBinaryName("media_kernels_frontend");

    const char *kernelNamesString = {
        "block_motion_estimate_intel;"};

    cl_program program = clCreateProgramWithBuiltInKernels(
        pContext,          // context
        1,                 // num_devices
        &testedClDevice,   // device_list
        kernelNamesString, // kernel_names
        &retVal);

    restoreBuiltInBinaryName();

    auto neoProgram = castToObject<Program>(program);
    auto builtinOptions = neoProgram->getOptions();
    auto it = builtinOptions.find("HW_NULL_CHECK");
    EXPECT_EQ(std::string::npos, it);

    clReleaseProgram(program);
}

TEST_F(clCreateProgramWithBuiltInVmeKernelsTests, GivenVmeBlockMotionEstimateKernelWhenCreatingProgramWithBuiltInKernelsThenCorrectDispatchBuilderAndFrontendKernelIsCreated) {
    cl_int retVal = CL_SUCCESS;

    overwriteBuiltInBinaryName("media_kernels_backend");
    Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockMotionEstimateIntel, *pClDevice);
    restoreBuiltInBinaryName();

    overwriteBuiltInBinaryName("media_kernels_frontend");

    const char *kernelNamesString = {
        "block_motion_estimate_intel;"};

    cl_program program = clCreateProgramWithBuiltInKernels(
        pContext,          // context
        1,                 // num_devices
        &testedClDevice,   // device_list
        kernelNamesString, // kernel_names
        &retVal);
    restoreBuiltInBinaryName();

    cl_kernel kernel = clCreateKernel(
        program,
        "block_motion_estimate_intel",
        &retVal);

    auto pMultiDeviceKernel = castToObject<MultiDeviceKernel>(kernel);
    auto kernNeo = pMultiDeviceKernel->getKernel(testedRootDeviceIndex);
    EXPECT_NE(nullptr, kernNeo->getKernelInfo().builtinDispatchBuilder);
    EXPECT_EQ(6U, kernNeo->getKernelArgsNumber());

    auto &vmeBuilder = Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockMotionEstimateIntel, *pClDevice);
    EXPECT_EQ(&vmeBuilder, kernNeo->getKernelInfo().builtinDispatchBuilder);

    clReleaseKernel(kernel);
    clReleaseProgram(program);
}

TEST_F(clCreateProgramWithBuiltInVmeKernelsTests, GivenVmeBlockAdvancedMotionEstimateKernelWhenCreatingProgramWithBuiltInKernelsThenCorrectDispatchBuilderAndFrontendKernelIsCreated) {
    cl_int retVal = CL_SUCCESS;

    overwriteBuiltInBinaryName("media_kernels_backend");
    Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel, *pClDevice);
    restoreBuiltInBinaryName();

    overwriteBuiltInBinaryName("media_kernels_frontend");

    const char *kernelNamesString = {
        "block_advanced_motion_estimate_check_intel;"};

    cl_program program = clCreateProgramWithBuiltInKernels(
        pContext,          // context
        1,                 // num_devices
        &testedClDevice,   // device_list
        kernelNamesString, // kernel_names
        &retVal);
    restoreBuiltInBinaryName();

    cl_kernel kernel = clCreateKernel(
        program,
        "block_advanced_motion_estimate_check_intel",
        &retVal);

    auto pMultiDeviceKernel = castToObject<MultiDeviceKernel>(kernel);
    auto kernNeo = pMultiDeviceKernel->getKernel(testedRootDeviceIndex);
    EXPECT_NE(nullptr, kernNeo->getKernelInfo().builtinDispatchBuilder);
    EXPECT_EQ(15U, kernNeo->getKernelArgsNumber());

    auto &vmeBuilder = Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel, *pClDevice);
    EXPECT_EQ(&vmeBuilder, kernNeo->getKernelInfo().builtinDispatchBuilder);

    clReleaseKernel(kernel);
    clReleaseProgram(program);
}

TEST_F(clCreateProgramWithBuiltInVmeKernelsTests, GivenVmeBlockAdvancedMotionEstimateBidirectionalCheckKernelWhenCreatingProgramWithBuiltInKernelsThenCorrectDispatchBuilderAndFrontendKernelIsCreated) {
    cl_int retVal = CL_SUCCESS;

    overwriteBuiltInBinaryName("media_kernels_backend");
    Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel, *pClDevice);
    restoreBuiltInBinaryName();

    overwriteBuiltInBinaryName("media_kernels_frontend");

    const char *kernelNamesString = {
        "block_advanced_motion_estimate_bidirectional_check_intel;"};

    cl_program program = clCreateProgramWithBuiltInKernels(
        pContext,          // context
        1,                 // num_devices
        &testedClDevice,   // device_list
        kernelNamesString, // kernel_names
        &retVal);
    restoreBuiltInBinaryName();

    cl_kernel kernel = clCreateKernel(
        program,
        "block_advanced_motion_estimate_bidirectional_check_intel",
        &retVal);

    auto pMultiDeviceKernel = castToObject<MultiDeviceKernel>(kernel);
    auto kernNeo = pMultiDeviceKernel->getKernel(testedRootDeviceIndex);
    EXPECT_NE(nullptr, kernNeo->getKernelInfo().builtinDispatchBuilder);
    EXPECT_EQ(20U, kernNeo->getKernelArgsNumber());

    auto ctxNeo = castToObject<Context>(pContext);
    auto &vmeBuilder = Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel, *ctxNeo->getDevice(0));
    EXPECT_EQ(&vmeBuilder, kernNeo->getKernelInfo().builtinDispatchBuilder);

    clReleaseKernel(kernel);
    clReleaseProgram(program);
}
} // namespace ULT
