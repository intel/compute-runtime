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

#include "cl_api_tests.h"

#include "unit_tests/fixtures/run_kernel_fixture.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/context/context.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/kernel/kernel.h"
#include "runtime/helpers/base_object.h"
#include "runtime/program/program.h"

using namespace OCLRT;

typedef api_tests clCreateProgramWithBuiltInKernelsTests;

namespace ULT {

TEST_F(clCreateProgramWithBuiltInKernelsTests, invalidArgs) {
    cl_int retVal = CL_SUCCESS;
    auto program = clCreateProgramWithBuiltInKernels(
        nullptr, // context
        0,       // num_devices
        nullptr, // device_list
        nullptr, // kernel_names
        &retVal);
    EXPECT_EQ(nullptr, program);
    EXPECT_NE(CL_SUCCESS, retVal);
}

TEST_F(clCreateProgramWithBuiltInKernelsTests, noKernels) {
    cl_int retVal = CL_SUCCESS;
    auto program = clCreateProgramWithBuiltInKernels(
        pContext, // context
        1,        // num_devices
        devices,  // device_list
        "",       // kernel_names
        &retVal);
    EXPECT_EQ(nullptr, program);
    EXPECT_NE(CL_SUCCESS, retVal);
}

TEST_F(clCreateProgramWithBuiltInKernelsTests, noDevice) {
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

TEST_F(clCreateProgramWithBuiltInKernelsTests, noRet) {
    auto program = clCreateProgramWithBuiltInKernels(
        pContext, // context
        1,        // num_devices
        devices,  // device_list
        "",       // kernel_names
        nullptr);
    EXPECT_EQ(nullptr, program);
}

TEST_F(clCreateProgramWithBuiltInKernelsTests, mediaKernels) {
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

    CompilerInterface::shutdown();
}

TEST_F(clCreateProgramWithBuiltInKernelsTests, mediaKernelsOptions) {
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

    CompilerInterface::shutdown();
}

TEST_F(clCreateProgramWithBuiltInKernelsTests, vmeBlockMotionEstimateKernelHasCorrectDispatchBuilderAndFrontendKernel) {
    cl_int retVal = CL_SUCCESS;

    auto pDev = castToObject<Device>(*devices);

    overwriteBuiltInBinaryName(pDev, "media_kernels_backend");
    pDev->getBuiltIns().getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockMotionEstimateIntel, *pContext, *pDev);
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
    auto &vmeBuilder = pDev->getBuiltIns().getBuiltinDispatchInfoBuilder(OCLRT::EBuiltInOps::VmeBlockMotionEstimateIntel, *ctxNeo, *ctxNeo->getDevice(0));
    EXPECT_EQ(&vmeBuilder, kernNeo->getKernelInfo().builtinDispatchBuilder);

    clReleaseKernel(kernel);
    clReleaseProgram(program);

    CompilerInterface::shutdown();
}

TEST_F(clCreateProgramWithBuiltInKernelsTests, vmeBlockAdvancedMotionEstimateCheckKernelHasCorrectDispatchBuilderAndFrontendKernel) {
    cl_int retVal = CL_SUCCESS;

    auto pDev = castToObject<Device>(*devices);

    overwriteBuiltInBinaryName(pDev, "media_kernels_backend");
    pDev->getBuiltIns().getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel, *pContext, *pDev);
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
    auto &vmeBuilder = pDev->getBuiltIns().getBuiltinDispatchInfoBuilder(OCLRT::EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel, *ctxNeo, *ctxNeo->getDevice(0));
    EXPECT_EQ(&vmeBuilder, kernNeo->getKernelInfo().builtinDispatchBuilder);

    clReleaseKernel(kernel);
    clReleaseProgram(program);

    CompilerInterface::shutdown();
}

TEST_F(clCreateProgramWithBuiltInKernelsTests, vmeBlockAdvancedMotionEstimateBidirectionalCheckKernelHasCorrectDispatchBuilderAndFrontendKernel) {
    cl_int retVal = CL_SUCCESS;

    auto pDev = castToObject<Device>(*devices);

    overwriteBuiltInBinaryName(pDev, "media_kernels_backend");
    pDev->getBuiltIns().getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel, *pContext, *pDev);
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
    auto &vmeBuilder = pDev->getBuiltIns().getBuiltinDispatchInfoBuilder(OCLRT::EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel, *ctxNeo, *ctxNeo->getDevice(0));
    EXPECT_EQ(&vmeBuilder, kernNeo->getKernelInfo().builtinDispatchBuilder);

    clReleaseKernel(kernel);
    clReleaseProgram(program);

    CompilerInterface::shutdown();
}
} // namespace ULT
