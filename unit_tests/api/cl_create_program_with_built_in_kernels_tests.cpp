/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/helpers/base_object.h"
#include "runtime/kernel/kernel.h"
#include "runtime/program/program.h"
#include "unit_tests/fixtures/run_kernel_fixture.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clCreateProgramWithBuiltInKernelsTests;

struct clCreateProgramWithBuiltInVmeKernelsTests : clCreateProgramWithBuiltInKernelsTests {
    void SetUp() override {
        clCreateProgramWithBuiltInKernelsTests::SetUp();
        if (!castToObject<Device>(devices[0])->getHardwareInfo().capabilityTable.supportsVme) {
            GTEST_SKIP();
        }
    }
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
        pContext, // context
        1,        // num_devices
        devices,  // device_list
        "",       // kernel_names
        &retVal);
    EXPECT_EQ(nullptr, program);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateProgramWithBuiltInKernelsTests, GivenNoDeviceWhenCreatingProgramWithBuiltInKernelsThenInvalidValueErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto program = clCreateProgramWithBuiltInKernels(
        pContext, // context
        0,        // num_devices
        devices,  // device_list
        "",       // kernel_names
        &retVal);
    EXPECT_EQ(nullptr, program);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateProgramWithBuiltInKernelsTests, GivenNoKernelsAndNoReturnWhenCreatingProgramWithBuiltInKernelsThenProgramIsNotCreated) {
    auto program = clCreateProgramWithBuiltInKernels(
        pContext, // context
        1,        // num_devices
        devices,  // device_list
        "",       // kernel_names
        nullptr);
    EXPECT_EQ(nullptr, program);
}

TEST_F(clCreateProgramWithBuiltInVmeKernelsTests, GivenValidMediaKernelsWhenCreatingProgramWithBuiltInKernelsThenProgramIsSuccessfullyCreated) {
    cl_int retVal = CL_SUCCESS;

    auto pDev = castToObject<Device>(*devices);
    overwriteBuiltInBinaryName(pDev, "media_kernels_frontend");

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
        devices,           // device_list
        kernelNamesString, // kernel_names
        &retVal);

    restoreBuiltInBinaryName(pDev);
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

    auto pDev = castToObject<Device>(*devices);
    overwriteBuiltInBinaryName(pDev, "media_kernels_frontend");

    const char *kernelNamesString = {
        "block_motion_estimate_intel;"};

    cl_program program = clCreateProgramWithBuiltInKernels(
        pContext,          // context
        1,                 // num_devices
        devices,           // device_list
        kernelNamesString, // kernel_names
        &retVal);

    restoreBuiltInBinaryName(pDev);

    auto neoProgram = castToObject<Program>(program);
    auto builtinOptions = neoProgram->getOptions();
    auto it = builtinOptions.find("HW_NULL_CHECK");
    EXPECT_EQ(std::string::npos, it);

    clReleaseProgram(program);
}

TEST_F(clCreateProgramWithBuiltInVmeKernelsTests, GivenVmeBlockMotionEstimateKernelWhenCreatingProgramWithBuiltInKernelsThenCorrectDispatchBuilderAndFrontendKernelIsCreated) {
    cl_int retVal = CL_SUCCESS;

    auto pDev = castToObject<Device>(*devices);

    overwriteBuiltInBinaryName(pDev, "media_kernels_backend");
    pDev->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockMotionEstimateIntel, *pContext, *pDev);
    restoreBuiltInBinaryName(pDev);

    overwriteBuiltInBinaryName(pDev, "media_kernels_frontend");

    const char *kernelNamesString = {
        "block_motion_estimate_intel;"};

    cl_program program = clCreateProgramWithBuiltInKernels(
        pContext,          // context
        1,                 // num_devices
        devices,           // device_list
        kernelNamesString, // kernel_names
        &retVal);
    restoreBuiltInBinaryName(pDev);

    cl_kernel kernel = clCreateKernel(
        program,
        "block_motion_estimate_intel",
        &retVal);

    auto kernNeo = castToObject<Kernel>(kernel);
    EXPECT_NE(nullptr, kernNeo->getKernelInfo().builtinDispatchBuilder);
    EXPECT_EQ(6U, kernNeo->getKernelArgsNumber());

    auto ctxNeo = castToObject<Context>(pContext);
    auto &vmeBuilder = pDev->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(NEO::EBuiltInOps::VmeBlockMotionEstimateIntel, *ctxNeo, *ctxNeo->getDevice(0));
    EXPECT_EQ(&vmeBuilder, kernNeo->getKernelInfo().builtinDispatchBuilder);

    clReleaseKernel(kernel);
    clReleaseProgram(program);
}

TEST_F(clCreateProgramWithBuiltInVmeKernelsTests, GivenVmeBlockAdvancedMotionEstimateKernelWhenCreatingProgramWithBuiltInKernelsThenCorrectDispatchBuilderAndFrontendKernelIsCreated) {
    cl_int retVal = CL_SUCCESS;

    auto pDev = castToObject<Device>(*devices);

    overwriteBuiltInBinaryName(pDev, "media_kernels_backend");
    pDev->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel, *pContext, *pDev);
    restoreBuiltInBinaryName(pDev);

    overwriteBuiltInBinaryName(pDev, "media_kernels_frontend");

    const char *kernelNamesString = {
        "block_advanced_motion_estimate_check_intel;"};

    cl_program program = clCreateProgramWithBuiltInKernels(
        pContext,          // context
        1,                 // num_devices
        devices,           // device_list
        kernelNamesString, // kernel_names
        &retVal);
    restoreBuiltInBinaryName(pDev);

    cl_kernel kernel = clCreateKernel(
        program,
        "block_advanced_motion_estimate_check_intel",
        &retVal);

    auto kernNeo = castToObject<Kernel>(kernel);
    EXPECT_NE(nullptr, kernNeo->getKernelInfo().builtinDispatchBuilder);
    EXPECT_EQ(15U, kernNeo->getKernelArgsNumber());

    auto ctxNeo = castToObject<Context>(pContext);
    auto &vmeBuilder = pDev->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(NEO::EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel, *ctxNeo, *ctxNeo->getDevice(0));
    EXPECT_EQ(&vmeBuilder, kernNeo->getKernelInfo().builtinDispatchBuilder);

    clReleaseKernel(kernel);
    clReleaseProgram(program);
}

TEST_F(clCreateProgramWithBuiltInVmeKernelsTests, GivenVmeBlockAdvancedMotionEstimateBidirectionalCheckKernelWhenCreatingProgramWithBuiltInKernelsThenCorrectDispatchBuilderAndFrontendKernelIsCreated) {
    cl_int retVal = CL_SUCCESS;

    auto pDev = castToObject<Device>(*devices);

    overwriteBuiltInBinaryName(pDev, "media_kernels_backend");
    pDev->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel, *pContext, *pDev);
    restoreBuiltInBinaryName(pDev);

    overwriteBuiltInBinaryName(pDev, "media_kernels_frontend");

    const char *kernelNamesString = {
        "block_advanced_motion_estimate_bidirectional_check_intel;"};

    cl_program program = clCreateProgramWithBuiltInKernels(
        pContext,          // context
        1,                 // num_devices
        devices,           // device_list
        kernelNamesString, // kernel_names
        &retVal);
    restoreBuiltInBinaryName(pDev);

    cl_kernel kernel = clCreateKernel(
        program,
        "block_advanced_motion_estimate_bidirectional_check_intel",
        &retVal);

    auto kernNeo = castToObject<Kernel>(kernel);
    EXPECT_NE(nullptr, kernNeo->getKernelInfo().builtinDispatchBuilder);
    EXPECT_EQ(20U, kernNeo->getKernelArgsNumber());

    auto ctxNeo = castToObject<Context>(pContext);
    auto &vmeBuilder = pDev->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(NEO::EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel, *ctxNeo, *ctxNeo->getDevice(0));
    EXPECT_EQ(&vmeBuilder, kernNeo->getKernelInfo().builtinDispatchBuilder);

    clReleaseKernel(kernel);
    clReleaseProgram(program);
}
} // namespace ULT
