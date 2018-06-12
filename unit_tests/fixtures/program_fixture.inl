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

#include "unit_tests/fixtures/program_fixture.h"

namespace OCLRT {

template <typename T>
void ProgramFixture::CreateProgramFromBinary(cl_context context,
                                             cl_device_id *pDeviceList,
                                             const std::string &binaryFileName,
                                             cl_int &retVal,
                                             const std::string &options) {
    Cleanup();
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
