/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/program_fixture.h"

#include "opencl/source/program/create.inl"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

namespace NEO {
void ProgramFixture::createProgramWithSource(Context *pContext,
                                             const std::string &sourceFileName) {
    cleanup();
    cl_int retVal = CL_SUCCESS;
    std::string testFile;

    testFile.append(clFiles);
    testFile.append(sourceFileName);
    ASSERT_EQ(true, fileExists(testFile));

    knownSource = loadDataFromFile(
        testFile.c_str(),
        knownSourceSize);

    ASSERT_NE(0u, knownSourceSize);
    ASSERT_NE(nullptr, knownSource);

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
                                             const std::string &binaryFileName,
                                             cl_int &retVal,
                                             const std::string &options) {
    retVal = CL_SUCCESS;

    std::string testFile;
    retrieveBinaryKernelFilename(testFile, binaryFileName + "_", ".bin", options);

    knownSource = loadDataFromFile(
        testFile.c_str(),
        knownSourceSize);

    ASSERT_NE(0u, knownSourceSize);
    ASSERT_NE(nullptr, knownSource);

    pProgram = Program::create<MockProgram>(
        pContext,
        deviceVector,
        &knownSourceSize,
        (const unsigned char **)&knownSource,
        nullptr,
        retVal);
}

void ProgramFixture::createProgramFromBinary(Context *pContext,
                                             const ClDeviceVector &deviceVector,
                                             const std::string &binaryFileName,
                                             const std::string &options) {
    cleanup();
    cl_int retVal = CL_SUCCESS;
    createProgramFromBinary(
        pContext,
        deviceVector,
        binaryFileName,
        retVal,
        options);

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
