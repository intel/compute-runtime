/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/program/program.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

namespace {
void CL_CALLBACK dummyProgramCallback(cl_program, void *) {}
} // namespace

TEST(ProgramIsValidCallbackTests, givenNullFuncAndNullUserDataWhenIsValidCallbackThenReturnsTrue) {
    EXPECT_TRUE(Program::isValidCallback(nullptr, nullptr));
}

TEST(ProgramIsValidCallbackTests, givenNonNullFuncAndNullUserDataWhenIsValidCallbackThenReturnsTrue) {
    EXPECT_TRUE(Program::isValidCallback(dummyProgramCallback, nullptr));
}

TEST(ProgramIsValidCallbackTests, givenNonNullFuncAndNonNullUserDataWhenIsValidCallbackThenReturnsTrue) {
    int userData = 42;
    EXPECT_TRUE(Program::isValidCallback(dummyProgramCallback, &userData));
}

TEST(ProgramIsValidCallbackTests, givenNullFuncAndNonNullUserDataWhenIsValidCallbackThenReturnsFalse) {
    int userData = 42;
    EXPECT_FALSE(Program::isValidCallback(nullptr, &userData));
}

TEST(ProgramNullHandleTests, givenNullProgramWhenRetainProgramThenReturnsCLInvalidProgram) {
    EXPECT_EQ(CL_INVALID_PROGRAM, clRetainProgram(nullptr));
}

TEST(ProgramNullHandleTests, givenNullProgramWhenReleaseProgramThenReturnsCLInvalidProgram) {
    EXPECT_EQ(CL_INVALID_PROGRAM, clReleaseProgram(nullptr));
}

TEST(ProgramNullHandleTests, givenNullProgramWhenGetProgramInfoThenReturnsCLInvalidProgram) {
    EXPECT_EQ(CL_INVALID_PROGRAM, clGetProgramInfo(nullptr, CL_PROGRAM_NUM_DEVICES, 0, nullptr, nullptr));
}

TEST(ProgramNullHandleTests, givenNullProgramWhenGetProgramBuildInfoThenReturnsCLInvalidProgram) {
    EXPECT_EQ(CL_INVALID_PROGRAM, clGetProgramBuildInfo(nullptr, nullptr, CL_PROGRAM_BUILD_STATUS, 0, nullptr, nullptr));
}

TEST(ProgramNullHandleTests, givenNullProgramWhenBuildProgramThenReturnsCLInvalidProgram) {
    EXPECT_EQ(CL_INVALID_PROGRAM, clBuildProgram(nullptr, 0, nullptr, nullptr, nullptr, nullptr));
}

TEST(CreateProgramTests, givenNullContextWhenCreateProgramWithSourceThenReturnsCLInvalidContext) {
    cl_int errcode = CL_SUCCESS;
    const char *source = "kernel void foo() {}";
    auto program = clCreateProgramWithSource(nullptr, 1, &source, nullptr, &errcode);
    EXPECT_EQ(nullptr, program);
    EXPECT_EQ(CL_INVALID_CONTEXT, errcode);
}

TEST(CreateProgramTests, givenNullStringsWhenCreateProgramWithSourceThenReturnsCLInvalidValue) {
    cl_int errcode = CL_SUCCESS;
    auto program = clCreateProgramWithSource(nullptr, 1, nullptr, nullptr, &errcode);
    EXPECT_EQ(nullptr, program);
    EXPECT_EQ(CL_INVALID_VALUE, errcode);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
