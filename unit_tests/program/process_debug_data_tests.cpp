/*
* Copyright (c) 2018, Intel Corporation
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

#include "unit_tests/program/program_tests.h"
#include "unit_tests/mocks/mock_program.h"
#include "program_debug_data.h"
#include "test.h"

#include <memory>
using namespace iOpenCL;
using namespace OCLRT;

TEST_F(ProgramTests, GivenProgramWithDebugDataForTwoKernelsWhenPorcessedThenDebugDataIsSetInKernelInfos) {
    const char kernelName1[] = "kernel1";
    const char kernelName2[] = "kernel2";
    uint32_t kernelNameSize = static_cast<uint32_t>(sizeof(kernelName1));
    uint32_t genIsaSize = 8;
    uint32_t visaSize = 8;
    size_t debugDataSize = sizeof(SProgramDebugDataHeaderIGC) + 2 * (sizeof(SKernelDebugDataHeaderIGC) + kernelNameSize + genIsaSize + visaSize);
    std::unique_ptr<char[]> debugData(new char[debugDataSize]);

    KernelInfo *kernelInfo1 = new KernelInfo();
    kernelInfo1->name = kernelName1;
    KernelInfo *kernelInfo2 = new KernelInfo();
    kernelInfo2->name = kernelName2;
    std::unique_ptr<MockProgram> program(new MockProgram());

    SProgramDebugDataHeaderIGC *programDebugHeader = reinterpret_cast<SProgramDebugDataHeaderIGC *>(debugData.get());
    programDebugHeader->NumberOfKernels = 2;

    SKernelDebugDataHeaderIGC *kernelDebugHeader = reinterpret_cast<SKernelDebugDataHeaderIGC *>(ptrOffset(programDebugHeader, sizeof(SProgramDebugDataHeaderIGC)));
    kernelDebugHeader->KernelNameSize = kernelNameSize;
    kernelDebugHeader->SizeGenIsaDbgInBytes = genIsaSize;
    kernelDebugHeader->SizeVisaDbgInBytes = visaSize;

    char *kernelName = reinterpret_cast<char *>(ptrOffset(kernelDebugHeader, sizeof(SKernelDebugDataHeaderIGC)));
    memcpy_s(kernelName, kernelNameSize, kernelName1, kernelNameSize);

    char *vIsa1 = (ptrOffset(kernelName, kernelNameSize));
    memset(vIsa1, 10, visaSize);

    char *genIsa1 = (ptrOffset(vIsa1, visaSize));
    memset(genIsa1, 20, genIsaSize);

    kernelDebugHeader = reinterpret_cast<SKernelDebugDataHeaderIGC *>(ptrOffset(vIsa1, genIsaSize + visaSize));
    kernelDebugHeader->KernelNameSize = kernelNameSize;
    kernelDebugHeader->SizeGenIsaDbgInBytes = genIsaSize;
    kernelDebugHeader->SizeVisaDbgInBytes = visaSize;
    kernelName = reinterpret_cast<char *>(ptrOffset(kernelDebugHeader, sizeof(SKernelDebugDataHeaderIGC)));

    memcpy_s(kernelName, kernelNameSize, kernelName2, kernelNameSize);

    char *vIsa2 = (ptrOffset(kernelName, kernelNameSize));
    memset(vIsa2, 10, visaSize);

    char *genIsa2 = (ptrOffset(vIsa2, visaSize));
    memset(genIsa2, 20, genIsaSize);

    program->storeDebugData(debugData.get(), debugDataSize);

    program->addKernelInfo(kernelInfo1);
    program->addKernelInfo(kernelInfo2);

    program->processDebugData();

    size_t programDebugDataSize = 0;
    EXPECT_EQ(genIsaSize, kernelInfo1->debugData.genIsaSize);
    EXPECT_EQ(visaSize, kernelInfo1->debugData.vIsaSize);
    EXPECT_EQ(ptrDiff(vIsa1, debugData.get()), ptrDiff(kernelInfo1->debugData.vIsa, program->getDebugDataBinary(programDebugDataSize)));
    EXPECT_EQ(ptrDiff(genIsa1, debugData.get()), ptrDiff(kernelInfo1->debugData.genIsa, program->getDebugDataBinary(programDebugDataSize)));

    EXPECT_EQ(genIsaSize, kernelInfo2->debugData.genIsaSize);
    EXPECT_EQ(visaSize, kernelInfo2->debugData.vIsaSize);
    EXPECT_EQ(ptrDiff(vIsa2, debugData.get()), ptrDiff(kernelInfo2->debugData.vIsa, program->getDebugDataBinary(programDebugDataSize)));
    EXPECT_EQ(ptrDiff(genIsa2, debugData.get()), ptrDiff(kernelInfo2->debugData.genIsa, program->getDebugDataBinary(programDebugDataSize)));
}