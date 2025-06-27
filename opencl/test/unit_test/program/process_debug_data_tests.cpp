/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_tests.h"

#include "program_debug_data.h"

#include <memory>

using namespace iOpenCL;
using namespace NEO;

TEST_F(ProgramTests, GivenProgramWithDebugDataForTwoKernelsWhenPorcessedThenDebugDataIsSetInKernelInfos) {
    const char kernelName1[] = "kernel1";
    const char kernelName2[] = "kernel2";
    uint32_t kernelNameSize = static_cast<uint32_t>(sizeof(kernelName1));
    uint32_t genIsaSize = 8;
    uint32_t visaSize = 8;
    size_t debugDataSize = sizeof(SProgramDebugDataHeaderIGC) + 2 * (sizeof(SKernelDebugDataHeaderIGC) + kernelNameSize + genIsaSize + visaSize);
    std::unique_ptr<char[]> debugData(new char[debugDataSize]);

    auto kernelInfo1 = new KernelInfo();
    kernelInfo1->kernelDescriptor.kernelMetadata.kernelName = kernelName1;
    auto kernelInfo2 = new KernelInfo();
    kernelInfo2->kernelDescriptor.kernelMetadata.kernelName = kernelName2;
    auto program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));

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

    program->buildInfos[rootDeviceIndex].debugData = makeCopy(debugData.get(), debugDataSize);
    program->buildInfos[rootDeviceIndex].debugDataSize = debugDataSize;

    program->addKernelInfo(kernelInfo1, rootDeviceIndex);
    program->addKernelInfo(kernelInfo2, rootDeviceIndex);

    program->processDebugData(rootDeviceIndex);
    EXPECT_EQ(genIsaSize, kernelInfo1->debugData.genIsaSize);
    EXPECT_EQ(visaSize, kernelInfo1->debugData.vIsaSize);
    EXPECT_EQ(ptrDiff(vIsa1, debugData.get()), ptrDiff(kernelInfo1->debugData.vIsa, program->getDebugData(rootDeviceIndex)));
    EXPECT_EQ(ptrDiff(genIsa1, debugData.get()), ptrDiff(kernelInfo1->debugData.genIsa, program->getDebugData(rootDeviceIndex)));

    EXPECT_EQ(genIsaSize, kernelInfo2->debugData.genIsaSize);
    EXPECT_EQ(visaSize, kernelInfo2->debugData.vIsaSize);
    EXPECT_EQ(ptrDiff(vIsa2, debugData.get()), ptrDiff(kernelInfo2->debugData.vIsa, program->getDebugData(rootDeviceIndex)));
    EXPECT_EQ(ptrDiff(genIsa2, debugData.get()), ptrDiff(kernelInfo2->debugData.genIsa, program->getDebugData(rootDeviceIndex)));
}

TEST_F(ProgramTests, GivenProgramWithoutDebugDataWhenPorcessedThenDebugDataIsNotSetInKernelInfo) {
    const char kernelName1[] = "kernel1";

    auto kernelInfo1 = new KernelInfo();
    kernelInfo1->kernelDescriptor.kernelMetadata.kernelName = kernelName1;
    auto program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));

    program->addKernelInfo(kernelInfo1, rootDeviceIndex);
    program->processDebugData(rootDeviceIndex);

    EXPECT_EQ(0u, kernelInfo1->debugData.genIsaSize);
    EXPECT_EQ(0u, kernelInfo1->debugData.vIsaSize);
    EXPECT_EQ(nullptr, program->getDebugData(rootDeviceIndex));
}
