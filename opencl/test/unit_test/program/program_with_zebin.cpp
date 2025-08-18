/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/program/program_with_zebin.h"

#include "shared/test/common/mocks/mock_modules_zebin.h"

#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

using namespace NEO;

void ProgramWithZebinFixture::SetUp() {
    ProgramTests::SetUp();
    program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));
}

void ProgramWithZebinFixture::TearDown() {
    program->setGlobalSurface(nullptr);
    program->setConstantSurface(nullptr);
    program->getKernelInfoArray(rootDeviceIndex).clear();
    program.reset();
    ProgramTests::TearDown();
}

void ProgramWithZebinFixture::addEmptyZebin(NEO::MockProgram *program) {
    auto zebin = ZebinTestData::ValidEmptyProgram<>();

    program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = zebin.storage.size();
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinary.reset(new char[zebin.storage.size()]);
    memcpy_s(program->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get(), program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize,
             zebin.storage.data(), zebin.storage.size());
}

void ProgramWithZebinFixture::populateProgramWithSegments(NEO::MockProgram *program) {
    constexpr bool createWithSharedGlobalConstSurfaces = false;
    populateProgramWithSegments(program, createWithSharedGlobalConstSurfaces);
}

void ProgramWithZebinFixture::populateProgramWithSegments(NEO::MockProgram *program, bool createWithSharedGlobalConstSurfaces) {
    kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;
    mockAlloc = std::make_unique<MockGraphicsAllocation>();
    kernelInfo->kernelAllocation = mockAlloc.get();

    program->addKernelInfo(kernelInfo.get(), rootDeviceIndex);

    globalSurface = std::make_unique<MockBuffer>();
    constantSurface = std::make_unique<MockBuffer>();

    if (createWithSharedGlobalConstSurfaces) {
        const size_t globalSurfaceChunkOffset = 128u;
        const size_t globalSurfaceChunkSize = globalSurface->mockGfxAllocation.getUnderlyingBufferSize() / 2;
        const size_t constantSurfaceChunkOffset = 512u;
        const size_t constantSurfaceChunkSize = constantSurface->mockGfxAllocation.getUnderlyingBufferSize() / 2;

        std::unique_ptr<SharedPoolAllocation> globalSurfacePoolAlloc = std::make_unique<SharedPoolAllocation>(&globalSurface->mockGfxAllocation, globalSurfaceChunkOffset, globalSurfaceChunkSize, nullptr);
        std::unique_ptr<SharedPoolAllocation> constantSurfacePoolAlloc = std::make_unique<SharedPoolAllocation>(&constantSurface->mockGfxAllocation, constantSurfaceChunkOffset, constantSurfaceChunkSize, nullptr);

        program->setGlobalSurface(std::move(globalSurfacePoolAlloc));
        program->setConstantSurface(std::move(constantSurfacePoolAlloc));

    } else {
        program->setGlobalSurface(&globalSurface->mockGfxAllocation);
        program->setConstantSurface(&constantSurface->mockGfxAllocation);
    }

    program->buildInfos[rootDeviceIndex].constStringSectionData.initData = &strings;
    program->buildInfos[rootDeviceIndex].constStringSectionData.size = sizeof(strings);
}

ProgramWithZebinFixture::ProgramWithZebinFixture() = default;
ProgramWithZebinFixture::~ProgramWithZebinFixture() = default;
