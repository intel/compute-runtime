/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/kernel_data_fixture.h"
#include "opencl/test/unit_test/helpers/gtest_helpers.h"

#include "patch_g7.h"

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

    const auto &eventPoolArg = pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress;
    EXPECT_EQ_VAL(allocateStatelessEventPoolSurface.DataParamOffset, eventPoolArg.stateless);
    EXPECT_EQ_VAL(allocateStatelessEventPoolSurface.DataParamSize, eventPoolArg.pointerSize);
    EXPECT_EQ_VAL(allocateStatelessEventPoolSurface.SurfaceStateHeapOffset, eventPoolArg.bindful);
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

    const auto &defaultQueueSurfaceAddress = pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress;
    EXPECT_EQ(allocateStatelessDefaultDeviceQueueSurface.DataParamOffset, defaultQueueSurfaceAddress.stateless);
    EXPECT_EQ(allocateStatelessDefaultDeviceQueueSurface.DataParamSize, defaultQueueSurfaceAddress.pointerSize);
    EXPECT_EQ(allocateStatelessDefaultDeviceQueueSurface.SurfaceStateHeapOffset, defaultQueueSurfaceAddress.bindful);
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

    ASSERT_GE(pKernelInfo->getExplicitArgs().size(), size_t(4u));
    EXPECT_TRUE(pKernelInfo->getArgDescriptorAt(3).getExtendedTypeInfo().isDeviceQueue);
    const auto &argAsPtr = pKernelInfo->getArgDescriptorAt(3).as<ArgDescPointer>();
    EXPECT_EQ(deviceQueueKernelArgument.DataParamOffset, argAsPtr.stateless);
    EXPECT_EQ(deviceQueueKernelArgument.DataParamSize, argAsPtr.pointerSize);
    EXPECT_EQ(deviceQueueKernelArgument.SurfaceStateHeapOffset, argAsPtr.bindful);
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

    EXPECT_EQ(pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueParentEvent, offsetSimdSize);
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

    EXPECT_EQ(pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.preferredWkgMultiple, offset);
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

    ASSERT_GE(pKernelInfo->getExplicitArgs().size(), size_t(argNum + 1));
    EXPECT_TRUE(pKernelInfo->getArgDescriptorAt(argNum).getExtendedTypeInfo().hasDeviceSideEnqueueExtendedDescriptor);
    auto deviceSideEnqueueDesc = reinterpret_cast<NEO::ArgDescriptorDeviceSideEnqueue *>(pKernelInfo->kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors[argNum].get());
    EXPECT_EQ(offsetObjectId, deviceSideEnqueueDesc->objectId);
}
