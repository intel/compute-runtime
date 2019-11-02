/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/program_fixture.h"

#include "runtime/program/create.inl"
#include "unit_tests/mocks/mock_program.h"

namespace NEO {
void ProgramFixture::CreateProgramWithSource(cl_context context,
                                             cl_device_id *deviceList,
                                             const std::string &sourceFileName) {
    Cleanup();
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
        context,
        1,
        sources,
        &knownSourceSize,
        retVal);

    ASSERT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

void ProgramFixture::CreateProgramFromBinary(cl_context context,
                                             cl_device_id *pDeviceList,
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
        context,
        1,
        pDeviceList,
        &knownSourceSize,
        (const unsigned char **)&knownSource,
        nullptr,
        retVal);
}

void ProgramFixture::CreateProgramFromBinary(cl_context pContext,
                                             cl_device_id *pDeviceList,
                                             const std::string &binaryFileName,
                                             const std::string &options) {
    Cleanup();
    cl_int retVal = CL_SUCCESS;
    CreateProgramFromBinary(
        pContext,
        pDeviceList,
        binaryFileName,
        retVal,
        options);

    ASSERT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

} // namespace NEO
