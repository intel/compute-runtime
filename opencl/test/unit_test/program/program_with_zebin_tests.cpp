/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/zebin/debug_zebin.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_with_zebin.h"

#include <memory>

using namespace NEO;

TEST_F(ProgramWithZebinFixture, givenNoZebinThenSegmentsAreEmpty) {
    auto segments = program->getZebinSegments(pClDevice->getRootDeviceIndex());

    EXPECT_EQ(std::numeric_limits<uintptr_t>::max(), segments.constData.address);
    EXPECT_EQ(0ULL, segments.constData.size);

    EXPECT_EQ(std::numeric_limits<uintptr_t>::max(), segments.varData.address);
    EXPECT_EQ(0ULL, segments.varData.size);

    EXPECT_EQ(std::numeric_limits<uintptr_t>::max(), segments.stringData.address);
    EXPECT_EQ(0ULL, segments.stringData.size);

    EXPECT_TRUE(segments.nameToSegMap.empty());
}

TEST_F(ProgramWithZebinFixture, givenZebinSegmentsThenSegmentsArePopulated) {
    populateProgramWithSegments(program.get());
    auto segments = program->getZebinSegments(rootDeviceIndex);

    auto checkGPUSeg = [](NEO::GraphicsAllocation *alloc, NEO::Zebin::Debug::Segments::Segment segment) {
        EXPECT_EQ(static_cast<uintptr_t>(alloc->getGpuAddress()), segment.address);
        EXPECT_EQ(static_cast<size_t>(alloc->getUnderlyingBufferSize()), segment.size);
    };
    checkGPUSeg(program->buildInfos[rootDeviceIndex].constantSurface, segments.constData);
    checkGPUSeg(program->buildInfos[rootDeviceIndex].globalSurface, segments.varData);
    checkGPUSeg(program->getKernelInfoArray(rootDeviceIndex)[0]->getGraphicsAllocation(), segments.nameToSegMap[ZebinTestData::ValidEmptyProgram<>::kernelName]);

    EXPECT_EQ(reinterpret_cast<uintptr_t>(program->buildInfos[rootDeviceIndex].constStringSectionData.initData), segments.stringData.address);
    EXPECT_EQ(reinterpret_cast<const char *>(program->buildInfos[rootDeviceIndex].constStringSectionData.initData), strings);
    EXPECT_EQ(program->buildInfos[rootDeviceIndex].constStringSectionData.size, sizeof(strings));
}

TEST_F(ProgramWithZebinFixture, givenNonEmptyDebugDataThenDebugZebinIsNotCreated) {
    addEmptyZebin(program.get());
    populateProgramWithSegments(program.get());
    program->buildInfos[rootDeviceIndex].debugDataSize = 8u;
    program->buildInfos[rootDeviceIndex].debugData.reset(nullptr);
    program->createDebugZebin(rootDeviceIndex);
    EXPECT_EQ(nullptr, program->buildInfos[rootDeviceIndex].debugData.get());
}

TEST_F(ProgramWithZebinFixture, givenEmptyDebugDataThenDebugZebinIsCreatedAndStoredInDebugData) {
    addEmptyZebin(program.get());
    populateProgramWithSegments(program.get());

    program->buildInfos[rootDeviceIndex].debugDataSize = 0u;
    program->buildInfos[rootDeviceIndex].debugData.reset(nullptr);
    program->createDebugZebin(rootDeviceIndex);
    EXPECT_NE(nullptr, program->buildInfos[rootDeviceIndex].debugData.get());
}

TEST_F(ProgramWithZebinFixture, givenEmptyDebugDataAndZebinBinaryFormatThenCreateDebugZebinAndReturnOnGetInfo) {
    addEmptyZebin(program.get());
    populateProgramWithSegments(program.get());
    program->buildInfos[rootDeviceIndex].debugDataSize = 0u;
    program->buildInfos[rootDeviceIndex].debugData.reset(nullptr);

    EXPECT_FALSE(program->wasCreateDebugZebinCalled);
    auto retVal = CL_INVALID_VALUE;
    size_t debugDataSize = 0;
    retVal = program->getInfo(CL_PROGRAM_DEBUG_INFO_SIZES_INTEL, sizeof(debugDataSize), &debugDataSize, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, program->buildInfos[rootDeviceIndex].debugData);
    EXPECT_TRUE(program->wasCreateDebugZebinCalled);

    program->wasCreateDebugZebinCalled = false;

    retVal = program->getInfo(CL_PROGRAM_DEBUG_INFO_SIZES_INTEL, sizeof(debugDataSize), &debugDataSize, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(program->wasCreateDebugZebinCalled);

    std::unique_ptr<char[]> debugData = std::make_unique<char[]>(debugDataSize);
    for (size_t n = 0; n < sizeof(debugData); n++) {
        debugData[n] = 0;
    }
    char *pDebugData = &debugData[0];
    size_t retData = 0;

    retVal = program->getInfo(CL_PROGRAM_DEBUG_INFO_INTEL, debugDataSize, &pDebugData, &retData);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(program->wasCreateDebugZebinCalled);

    program->buildInfos[rootDeviceIndex].debugDataSize = 0u;
    program->buildInfos[rootDeviceIndex].debugData.reset(nullptr);

    std::unique_ptr<char[]> debugData2 = std::make_unique<char[]>(debugDataSize);
    for (size_t n = 0; n < sizeof(debugData2); n++) {
        debugData2[n] = 0;
    }
    char *pDebugData2 = &debugData2[0];
    size_t retData2 = 0;
    retVal = program->getInfo(CL_PROGRAM_DEBUG_INFO_INTEL, debugDataSize, &pDebugData2, &retData2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(program->wasCreateDebugZebinCalled);

    cl_uint numDevices;
    retVal = clGetProgramInfo(program.get(), CL_PROGRAM_NUM_DEVICES, sizeof(numDevices), &numDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(numDevices * sizeof(debugData), retData);
}

TEST_F(ProgramWithZebinFixture, givenZebinFormatAndDebuggerNotAvailableWhenCreatingDebugDataThenCreateDebugZebinIsCalled) {
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(nullptr);

    addEmptyZebin(program.get());
    populateProgramWithSegments(program.get());
    auto &buildInfo = program->buildInfos[rootDeviceIndex];
    buildInfo.debugDataSize = 0u;
    buildInfo.debugData.reset(nullptr);
    for (auto &device : program->getDevices()) {
        program->createDebugData(device);
    }
    EXPECT_TRUE(program->wasCreateDebugZebinCalled);
    EXPECT_FALSE(program->wasProcessDebugDataCalled);
    EXPECT_NE(nullptr, program->buildInfos[rootDeviceIndex].debugData);
    EXPECT_GT(program->buildInfos[rootDeviceIndex].debugDataSize, 0u);
}