/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/program_fixture.h"

#include "opencl/source/program/create.inl"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

namespace NEO {

void ProgramFixture::createProgramWithSource(Context *pContext) {
    cleanup();
    cl_int retVal = CL_SUCCESS;

    const char *sampleKernel = "example_kernel(){}";
    knownSourceSize = std::strlen(sampleKernel) + 1;
    knownSource = std::make_unique<char[]>(knownSourceSize);
    std::copy(sampleKernel, sampleKernel + knownSourceSize, knownSource.get());

    const char *sources[1] = {knownSource.get()};
    pProgram = Program::create<MockProgram>(
        pContext,
        1,
        sources,
        &knownSourceSize,
        retVal);

    ASSERT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

void ProgramFixture::cleanup() {
    if (pProgram != nullptr) {
        pProgram->release();
    }
    knownSource.reset();
}

void ProgramFixture::createProgramFromBinary(Context *pContext,
                                             const ClDeviceVector &deviceVector,
                                             const unsigned char **binary,
                                             const size_t *binarySize) {
    cleanup();
    cl_int retVal = CL_SUCCESS;

    pProgram = Program::create<MockProgram>(
        pContext,
        deviceVector,
        binarySize,
        binary,
        nullptr,
        retVal);

    ASSERT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

NEOProgramFixture::NEOProgramFixture() = default;
NEOProgramFixture::~NEOProgramFixture() = default;

void NEOProgramFixture::setUp() {
    context = std::make_unique<MockContext>();
    program = std::make_unique<MockNeoProgram>(context.get(), false, context->getDevices());
}

void NEOProgramFixture::tearDown() {}

} // namespace NEO
