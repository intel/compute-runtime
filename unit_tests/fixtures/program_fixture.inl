/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/program_fixture.h"

namespace OCLRT {

template <typename T>
void ProgramFixture::CreateProgramFromBinary(cl_context context,
                                             cl_device_id *pDeviceList,
                                             const std::string &binaryFileName,
                                             cl_int &retVal,
                                             const std::string &options) {
    retVal = CL_SUCCESS;

    std::string testFile;
    retrieveBinaryKernelFilename(testFile, binaryFileName + "_", ".bin", options);

    knownSourceSize = loadDataFromFile(
        testFile.c_str(),
        knownSource);

    ASSERT_NE(0u, knownSourceSize);
    ASSERT_NE(nullptr, knownSource);

    pProgram = Program::create<T>(
        context,
        1,
        pDeviceList,
        &knownSourceSize,
        (const unsigned char **)&knownSource,
        nullptr,
        retVal);
}

template <typename T>
void ProgramFixture::CreateProgramFromBinary(cl_context pContext,
                                             cl_device_id *pDeviceList,
                                             const std::string &binaryFileName,
                                             const std::string &options) {
    Cleanup();
    cl_int retVal = CL_SUCCESS;
    CreateProgramFromBinary<T>(
        pContext,
        pDeviceList,
        binaryFileName,
        retVal,
        options);

    ASSERT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);
}
} // namespace OCLRT
