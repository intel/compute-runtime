/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/kernel_data_fixture.h"
#include "opencl/test/unit_test/helpers/gtest_helpers.h"

TEST_F(KernelDataTest, GIVENpatchTokenAllocateStatelessEventPoolSurfaceWHENdecodeTokensTHENtokenLocatedInPatchInfo) {
    iOpenCL::SPatchAllocateStatelessEventPoolSurface allocateStatelessEventPoolSurface;
    allocateStatelessEventPoolSurface.Token = PATCH_TOKEN_ALLOCATE_STATELESS_EVENT_POOL_SURFACE;
    allocateStatelessEventPoolSurface.Size = sizeof(SPatchAllocateStatelessEventPoolSurface);

    allocateStatelessEventPoolSurface.DataParamSize = 7;
    allocateStatelessEventPoolSurface.DataParamOffset = 0xABC;
    allocateStatelessEventPoolSurface.SurfaceStateHeapOffset = 0xDEF;

    pPatchList = &allocateStatelessEventPoolSurface;
    patchListSize = allocateStatelessEventPoolSurface.Size;

    buildAndDecode();

    EXPECT_EQ_VAL(allocateStatelessEventPoolSurface.Token,
                  pKernelInfo->patchInfo.pAllocateStatelessEventPoolSurface->Token);
    EXPECT_EQ_VAL(allocateStatelessEventPoolSurface.DataParamOffset,
                  pKernelInfo->patchInfo.pAllocateStatelessEventPoolSurface->DataParamOffset);
    EXPECT_EQ_VAL(allocateStatelessEventPoolSurface.DataParamSize,
                  pKernelInfo->patchInfo.pAllocateStatelessEventPoolSurface->DataParamSize);
    EXPECT_EQ_VAL(allocateStatelessEventPoolSurface.SurfaceStateHeapOffset,
                  pKernelInfo->patchInfo.pAllocateStatelessEventPoolSurface->SurfaceStateHeapOffset);
}

TEST_F(KernelDataTest, GIVENpatchTokenAllocateStatelessDefaultDeviceQueueSurfaceWHENdecodeTokensTHENtokenLocatedInPatchInfo) {
    iOpenCL::SPatchAllocateStatelessDefaultDeviceQueueSurface allocateStatelessDefaultDeviceQueueSurface;
    allocateStatelessDefaultDeviceQueueSurface.Token = PATCH_TOKEN_ALLOCATE_STATELESS_DEFAULT_DEVICE_QUEUE_SURFACE;
    allocateStatelessDefaultDeviceQueueSurface.Size = sizeof(SPatchAllocateStatelessDefaultDeviceQueueSurface);

    allocateStatelessDefaultDeviceQueueSurface.DataParamSize = 7;
    allocateStatelessDefaultDeviceQueueSurface.DataParamOffset = 0xABC;
    allocateStatelessDefaultDeviceQueueSurface.SurfaceStateHeapOffset = 0xDEF;

    pPatchList = &allocateStatelessDefaultDeviceQueueSurface;
    patchListSize = allocateStatelessDefaultDeviceQueueSurface.Size;

    buildAndDecode();

    EXPECT_EQ(allocateStatelessDefaultDeviceQueueSurface.Token,
              pKernelInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->Token);
    EXPECT_EQ(allocateStatelessDefaultDeviceQueueSurface.DataParamOffset,
              pKernelInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->DataParamOffset);
    EXPECT_EQ(allocateStatelessDefaultDeviceQueueSurface.DataParamSize,
              pKernelInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->DataParamSize);
    EXPECT_EQ(allocateStatelessDefaultDeviceQueueSurface.SurfaceStateHeapOffset,
              pKernelInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->SurfaceStateHeapOffset);
}

TEST_F(KernelDataTest, GIVENpatchTokenStatelessDeviceQueueKernelArgumentWHENdecodeTokensTHENapropriateKernelArgInfoFilled) {
    iOpenCL::SPatchStatelessDeviceQueueKernelArgument deviceQueueKernelArgument;
    deviceQueueKernelArgument.Token = PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT;
    deviceQueueKernelArgument.Size = sizeof(SPatchStatelessDeviceQueueKernelArgument);

    deviceQueueKernelArgument.ArgumentNumber = 3;
    deviceQueueKernelArgument.DataParamSize = 7;
    deviceQueueKernelArgument.DataParamOffset = 0xABC;
    deviceQueueKernelArgument.SurfaceStateHeapOffset = 0xDEF;

    pPatchList = &deviceQueueKernelArgument;
    patchListSize = deviceQueueKernelArgument.Size;

    buildAndDecode();

    ASSERT_GE(pKernelInfo->kernelArgInfo.size(), size_t(4u));
    EXPECT_EQ(pKernelInfo->kernelArgInfo[3].isDeviceQueue, true);
    EXPECT_EQ(pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector[0].crossthreadOffset,
              deviceQueueKernelArgument.DataParamOffset);
    EXPECT_EQ(pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector[0].size,
              deviceQueueKernelArgument.DataParamSize);
    EXPECT_EQ(pKernelInfo->kernelArgInfo[3].offsetHeap,
              deviceQueueKernelArgument.SurfaceStateHeapOffset);
}

TEST_F(KernelDataTest, GIVENdataParameterParentEventWHENdecodeTokensTHENoffsetLocatedInWorkloadInfo) {
    const uint32_t offsetSimdSize = 0xABC;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_PARENT_EVENT;
    dataParameterToken.ArgumentNumber = 0;
    dataParameterToken.Offset = offsetSimdSize;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = 0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    EXPECT_EQ(pKernelInfo->workloadInfo.parentEventOffset, offsetSimdSize);
}

TEST_F(KernelDataTest, GIVENdataParameterPreferredWorkgroupMultipleTokenWHENbinaryIsdecodedTHENcorrectOffsetIsAssigned) {
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

    EXPECT_EQ(pKernelInfo->workloadInfo.preferredWkgMultipleOffset, offset);
}

TEST_F(KernelDataTest, GIVENdataParameterObjectIdWHENdecodeTokensTHENoffsetLocatedInKernelArgInfo) {
    const uint32_t offsetObjectId = 0xABC;
    const uint32_t argNum = 7;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_OBJECT_ID;
    dataParameterToken.ArgumentNumber = argNum;
    dataParameterToken.Offset = offsetObjectId;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = 0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_GE(pKernelInfo->kernelArgInfo.size(), size_t(argNum + 1));
    EXPECT_EQ(pKernelInfo->kernelArgInfo[argNum].offsetObjectId, offsetObjectId);
}

TEST_F(KernelDataTest, GIVENdataParameterChildSimdSizeWHENdecodeTokensTHENchildsIdsStoredInKernelInfoWithOffset) {
    SPatchDataParameterBuffer patchList[3];
    uint32_t childrenKernelIds[3] = {7, 14, 21};
    uint32_t childrenSimdSizeOffsets[3] = {0x77, 0xAB, 0xCD};

    for (int i = 0; i < 3; i++) {
        patchList[i].Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
        patchList[i].Size = sizeof(SPatchDataParameterBuffer);
        patchList[i].Type = DATA_PARAMETER_CHILD_BLOCK_SIMD_SIZE;
        patchList[i].ArgumentNumber = childrenKernelIds[i];
        patchList[i].Offset = childrenSimdSizeOffsets[i];
        patchList[i].DataSize = sizeof(uint32_t);
        patchList[i].SourceOffset = 0;
    }

    pPatchList = patchList;
    patchListSize = sizeof(patchList);

    buildAndDecode();

    ASSERT_GE(pKernelInfo->childrenKernelsIdOffset.size(), size_t(3u));
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(pKernelInfo->childrenKernelsIdOffset[i].first, childrenKernelIds[i]);
        EXPECT_EQ(pKernelInfo->childrenKernelsIdOffset[i].second, childrenSimdSizeOffsets[i]);
    }
}
