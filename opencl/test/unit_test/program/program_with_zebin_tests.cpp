/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_buffer.h"
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

    auto checkGPUSeg = [](NEO::GraphicsAllocation *alloc, NEO::Debug::Segments::Segment segment) {
        EXPECT_EQ(static_cast<uintptr_t>(alloc->getGpuAddress()), segment.address);
        EXPECT_EQ(static_cast<size_t>(alloc->getUnderlyingBufferSize()), segment.size);
    };
    checkGPUSeg(program->buildInfos[rootDeviceIndex].constantSurface, segments.constData);
    checkGPUSeg(program->buildInfos[rootDeviceIndex].globalSurface, segments.varData);
    checkGPUSeg(program->getKernelInfoArray(rootDeviceIndex)[0]->getGraphicsAllocation(), segments.nameToSegMap["kernel1"]);

    EXPECT_EQ(reinterpret_cast<uintptr_t>(program->buildInfos[rootDeviceIndex].constStringSectionData.initData), segments.stringData.address);
    EXPECT_EQ(reinterpret_cast<const char *>(program->buildInfos[rootDeviceIndex].constStringSectionData.initData), strings);
    EXPECT_EQ(program->buildInfos[rootDeviceIndex].constStringSectionData.size, sizeof(strings));
}

TEST_F(ProgramWithZebinFixture, givenNonEmptyDebugDataThenDebugZebinIsNotCreated) {
    addEmptyZebin(program.get());
    populateProgramWithSegments(program.get());
    program->debugDataSize = 8u;
    program->debugData.reset(nullptr);
    program->createDebugZebin(rootDeviceIndex);
    EXPECT_EQ(nullptr, program->debugData.get());
}

TEST_F(ProgramWithZebinFixture, givenEmptyDebugDataThenDebugZebinIsCreatedAndStoredInDebugData) {
    addEmptyZebin(program.get());
    populateProgramWithSegments(program.get());

    program->debugDataSize = 0u;
    program->debugData.reset(nullptr);
    program->createDebugZebin(rootDeviceIndex);
    EXPECT_NE(nullptr, program->debugData.get());
}

TEST_F(ProgramWithDebugDataCreationFixture, givenZebinaryFormatInCreateDebugDataThenCreateDebugZebinIsCalled) {
    addEmptyZebin(programWithDebugDataCreation.get());
    programWithDebugDataCreation->createDebugData(rootDeviceIndex);
    EXPECT_TRUE(programWithDebugDataCreation->wasCreateDebugZebinCalled);
    EXPECT_FALSE(programWithDebugDataCreation->wasProcessDebugDataCalled);
}

TEST_F(ProgramWithDebugDataCreationFixture, givenNonZebinaryFormatInCreateDebugDataThenProcessDebugDataIsCalled) {
    size_t fakeBinarySize = 8u;
    programWithDebugDataCreation->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = fakeBinarySize;
    programWithDebugDataCreation->buildInfos[rootDeviceIndex].unpackedDeviceBinary.reset(new char[fakeBinarySize]);
    programWithDebugDataCreation->createDebugData(rootDeviceIndex);
    EXPECT_FALSE(programWithDebugDataCreation->wasCreateDebugZebinCalled);
    EXPECT_TRUE(programWithDebugDataCreation->wasProcessDebugDataCalled);
}
