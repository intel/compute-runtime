/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/program/program_with_zebin.h"

#include "shared/test/common/mocks/mock_modules_zebin.h"

#include "opencl/test/unit_test/mocks/mock_buffer.h"

using namespace NEO;

void ProgramWithZebinFixture::SetUp() {
    ProgramTests::SetUp();
    program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));
}

void ProgramWithZebinFixture::TearDown() {
    program->setGlobalSurface(nullptr);
    program->setConstantSurface(nullptr);
    program->getKernelInfoArray(rootDeviceIndex).clear();
    ProgramTests::TearDown();
}

void ProgramWithZebinFixture::addEmptyZebin(NEO::MockProgram *program) {
    auto zebin = ZebinTestData::ValidEmptyProgram();

    program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = zebin.storage.size();
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinary.reset(new char[zebin.storage.size()]);
    memcpy_s(program->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get(), program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize,
             zebin.storage.data(), zebin.storage.size());
}

void ProgramWithZebinFixture::populateProgramWithSegments(NEO::MockProgram *program) {
    kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram::kernelName;
    mockAlloc = std::make_unique<MockGraphicsAllocation>();
    kernelInfo->kernelAllocation = mockAlloc.get();

    program->addKernelInfo(kernelInfo.get(), rootDeviceIndex);

    globalSurface = std::make_unique<MockBuffer>();
    constantSurface = std::make_unique<MockBuffer>();
    program->setGlobalSurface(&globalSurface->mockGfxAllocation);
    program->setConstantSurface(&constantSurface->mockGfxAllocation);

    program->buildInfos[rootDeviceIndex].constStringSectionData.initData = &strings;
    program->buildInfos[rootDeviceIndex].constStringSectionData.size = sizeof(strings);
}