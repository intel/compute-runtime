/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/gtest_helpers.h"

#include "opencl/test/unit_test/fixtures/kernel_data_fixture.h"

#include "patch_g7.h"

TEST_F(KernelDataTest, givenPatchTokenAllocateStatelessEventPoolSurfaceWhenDecodeTokensThenTokenLocatedInPatchInfo) {
    iOpenCL::SPatchAllocateStatelessEventPoolSurface allocateStatelessEventPoolSurface;
    allocateStatelessEventPoolSurface.Token = PATCH_TOKEN_ALLOCATE_STATELESS_EVENT_POOL_SURFACE;
    allocateStatelessEventPoolSurface.Size = sizeof(SPatchAllocateStatelessEventPoolSurface);

    allocateStatelessEventPoolSurface.DataParamSize = 7;
    allocateStatelessEventPoolSurface.DataParamOffset = 0xABC;
    allocateStatelessEventPoolSurface.SurfaceStateHeapOffset = 0xDEF;

    pPatchList = &allocateStatelessEventPoolSurface;
    patchListSize = allocateStatelessEventPoolSurface.Size;

    buildAndDecode();

    const auto &eventPoolArg = pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress;
    EXPECT_EQ_VAL(allocateStatelessEventPoolSurface.DataParamOffset, eventPoolArg.stateless);
    EXPECT_EQ_VAL(allocateStatelessEventPoolSurface.DataParamSize, eventPoolArg.pointerSize);
    EXPECT_EQ_VAL(allocateStatelessEventPoolSurface.SurfaceStateHeapOffset, eventPoolArg.bindful);
}

TEST_F(KernelDataTest, givenDataParameterPreferredWorkgroupMultipleTokenWhenBinaryIsdecodedThenCorrectOffsetIsAssigned) {
    const uint32_t offset = 0x100;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_PREFERRED_WORKGROUP_MULTIPLE;
    dataParameterToken.ArgumentNumber = 0;
    dataParameterToken.Offset = offset;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = 0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    EXPECT_EQ(pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.preferredWkgMultiple, offset);
}
