/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/kernel/kernel_arg_descriptor_extended_device_side_enqueue.h"
#include "shared/source/kernel/kernel_arg_descriptor_extended_vme.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"
#include "shared/test/unit_test/device_binary_format/patchtokens_tests.h"

#include "test.h"

TEST(KernelDescriptorFromPatchtokens, GivenEmptyInputKernelFromPatchtokensThenOnlySetsUpPointerSize) {
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens;
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader;
    kernelTokens.header = &kernelHeader;
    NEO::KernelDescriptor kernelDescriptor;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.gpuPointerSize, 4);
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 8);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.gpuPointerSize, 8);
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelFromPatchtokensWhenKernelNameIsSpecifiedThenItIsCopiedIntoKernelDescriptor) {
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens;
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader;
    kernelTokens.header = &kernelHeader;
    NEO::KernelDescriptor kernelDescriptor;
    kernelTokens.name = "awesome_kern0";
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_STREQ("awesome_kern0", kernelDescriptor.kernelMetadata.kernelName.c_str());
}

TEST(KernelDescriptorFromPatchtokens, GivenExecutionEnvironmentThenSetsProperPartsOfDescriptor) {
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens;
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader;
    kernelTokens.header = &kernelHeader;
    NEO::KernelDescriptor kernelDescriptor;

    iOpenCL::SPatchExecutionEnvironment execEnv = {};
    kernelTokens.tokens.executionEnvironment = &execEnv;

    execEnv.CompiledForGreaterThan4GBBuffers = false;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(NEO::KernelDescriptor::BindfulAndStateless, kernelDescriptor.kernelAttributes.bufferAddressingMode);
    EXPECT_FALSE(kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());

    execEnv.CompiledForGreaterThan4GBBuffers = true;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(NEO::KernelDescriptor::Stateless, kernelDescriptor.kernelAttributes.bufferAddressingMode);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());

    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0]);
    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1]);
    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2]);
    execEnv.RequiredWorkGroupSizeX = 2;
    execEnv.RequiredWorkGroupSizeY = 3;
    execEnv.RequiredWorkGroupSizeZ = 5;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(2U, kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0]);
    EXPECT_EQ(3U, kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1]);
    EXPECT_EQ(5U, kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2]);

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.requiresWorkgroupWalkOrder);
    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.workgroupWalkOrder[0]);
    EXPECT_EQ(1U, kernelDescriptor.kernelAttributes.workgroupWalkOrder[1]);
    EXPECT_EQ(2U, kernelDescriptor.kernelAttributes.workgroupWalkOrder[2]);
    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[0]);
    EXPECT_EQ(1U, kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[1]);
    EXPECT_EQ(2U, kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[2]);
    constexpr auto dimensionSize = 2;
    execEnv.WorkgroupWalkOrderDims = (0 << (dimensionSize * 2)) | (2 << (dimensionSize * 1)) | (1 << (dimensionSize * 0));
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.requiresWorkgroupWalkOrder);
    EXPECT_EQ(1U, kernelDescriptor.kernelAttributes.workgroupWalkOrder[0]);
    EXPECT_EQ(2U, kernelDescriptor.kernelAttributes.workgroupWalkOrder[1]);
    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.workgroupWalkOrder[2]);
    EXPECT_EQ(2U, kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[0]);
    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[1]);
    EXPECT_EQ(1U, kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[2]);

    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.simdSize);
    execEnv.LargestCompiledSIMDSize = 32;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(32U, kernelDescriptor.kernelAttributes.simdSize);

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue);
    execEnv.HasDeviceEnqueue = 1U;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue);

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.usesBarriers);
    execEnv.HasBarriers = 1U;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.usesBarriers);

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption);
    execEnv.DisableMidThreadPreemption = 1U;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption);

    EXPECT_EQ(0U, kernelDescriptor.kernelMetadata.compiledSubGroupsNumber);
    execEnv.CompiledSubGroupsNumber = 8U;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(8U, kernelDescriptor.kernelMetadata.compiledSubGroupsNumber);

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages);
    execEnv.UsesFencesForReadWriteImages = 1U;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages);

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress);
    execEnv.SubgroupIndependentForwardProgressRequired = 1U;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress);

    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.numGrfRequired);
    execEnv.NumGRFRequired = 128U;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(128, kernelDescriptor.kernelAttributes.numGrfRequired);

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.useGlobalAtomics);
    execEnv.HasGlobalAtomics = 1U;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.useGlobalAtomics);

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.usesStatelessWrites);
}

TEST(KernelDescriptorFromPatchtokens, GivenThreadPayloadThenSetsProperPartsOfDescriptor) {
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens;
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader;
    kernelTokens.header = &kernelHeader;
    NEO::KernelDescriptor kernelDescriptor;

    iOpenCL::SPatchThreadPayload threadPayload = {};
    kernelTokens.tokens.threadPayload = &threadPayload;

    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.perThreadDataHeaderIsPresent);
    threadPayload.HeaderPresent = 1;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.perThreadDataHeaderIsPresent);

    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.numLocalIdChannels);
    threadPayload.LocalIDXPresent = 1;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(1U, kernelDescriptor.kernelAttributes.numLocalIdChannels);
    threadPayload.LocalIDYPresent = 1;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(2U, kernelDescriptor.kernelAttributes.numLocalIdChannels);
    threadPayload.LocalIDZPresent = 1;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(3U, kernelDescriptor.kernelAttributes.numLocalIdChannels);

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.usesFlattenedLocalIds);
    threadPayload.LocalIDFlattenedPresent = 1;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.usesFlattenedLocalIds);

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.perThreadDataUnusedGrfIsPresent);
    threadPayload.UnusedPerThreadConstantPresent = 1;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.perThreadDataUnusedGrfIsPresent);

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.passInlineData);
    threadPayload.PassInlineData = 1;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.passInlineData);

    EXPECT_EQ(0U, kernelDescriptor.entryPoints.skipPerThreadDataLoad);
    threadPayload.OffsetToSkipPerThreadDataLoad = 16;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(16U, kernelDescriptor.entryPoints.skipPerThreadDataLoad);

    EXPECT_EQ(0U, kernelDescriptor.entryPoints.skipSetFFIDGP);
    threadPayload.OffsetToSkipSetFFIDGP = 28;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(28U, kernelDescriptor.entryPoints.skipSetFFIDGP);
}

TEST(KernelDescriptorFromPatchtokens, GivenImplicitArgsThenSetsProperPartsOfDescriptor) {
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens;
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader;
    kernelTokens.header = &kernelHeader;
    NEO::KernelDescriptor kernelDescriptor;

    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.samplerTable.borderColor));
    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.samplerTable.numSamplers);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.samplerTable.tableOffset));
    iOpenCL::SPatchSamplerStateArray samplerStateArray = {};
    samplerStateArray.BorderColorOffset = 2;
    samplerStateArray.Count = 3;
    samplerStateArray.Offset = 5;
    kernelTokens.tokens.samplerStateArray = &samplerStateArray;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(samplerStateArray.BorderColorOffset, kernelDescriptor.payloadMappings.samplerTable.borderColor);
    EXPECT_EQ(samplerStateArray.Count, kernelDescriptor.payloadMappings.samplerTable.numSamplers);
    EXPECT_EQ(samplerStateArray.Offset, kernelDescriptor.payloadMappings.samplerTable.tableOffset);
    kernelTokens.tokens.samplerStateArray = nullptr;

    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.bindingTable.numEntries);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.bindingTable.tableOffset));
    iOpenCL::SPatchBindingTableState bindingTableState = {};
    bindingTableState.Count = 2;
    bindingTableState.Offset = 3;
    kernelTokens.tokens.bindingTableState = &bindingTableState;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(bindingTableState.Count, kernelDescriptor.payloadMappings.bindingTable.numEntries);
    EXPECT_EQ(bindingTableState.Offset, kernelDescriptor.payloadMappings.bindingTable.tableOffset);
    kernelTokens.tokens.bindingTableState = nullptr;

    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.slmInlineSize);
    iOpenCL::SPatchAllocateLocalSurface allocateLocalSurface = {};
    allocateLocalSurface.TotalInlineLocalMemorySize = 64;
    kernelTokens.tokens.allocateLocalSurface = &allocateLocalSurface;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(allocateLocalSurface.TotalInlineLocalMemorySize, kernelDescriptor.kernelAttributes.slmInlineSize);
    kernelTokens.tokens.allocateLocalSurface = nullptr;

    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.perThreadScratchSize[0]);
    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.perThreadScratchSize[1]);
    iOpenCL::SPatchMediaVFEState mediaVfeState0 = {};
    mediaVfeState0.PerThreadScratchSpace = 128;
    iOpenCL::SPatchMediaVFEState mediaVfeState1 = {};
    mediaVfeState0.PerThreadScratchSpace = 256;
    kernelTokens.tokens.mediaVfeState[0] = &mediaVfeState0;
    kernelTokens.tokens.mediaVfeState[1] = &mediaVfeState1;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(mediaVfeState0.PerThreadScratchSpace, kernelDescriptor.kernelAttributes.perThreadScratchSize[0]);
    EXPECT_EQ(mediaVfeState1.PerThreadScratchSpace, kernelDescriptor.kernelAttributes.perThreadScratchSize[1]);
    kernelTokens.tokens.mediaVfeState[0] = nullptr;
    kernelTokens.tokens.mediaVfeState[1] = nullptr;

    EXPECT_EQ(0U, kernelDescriptor.kernelMetadata.deviceSideEnqueueBlockInterfaceDescriptorOffset);
    iOpenCL::SPatchInterfaceDescriptorData interfaceDescriptorData = {};
    interfaceDescriptorData.Offset = 4096;
    kernelTokens.tokens.interfaceDescriptorData = &interfaceDescriptorData;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(interfaceDescriptorData.Offset, kernelDescriptor.kernelMetadata.deviceSideEnqueueBlockInterfaceDescriptorOffset);
    kernelTokens.tokens.interfaceDescriptorData = nullptr;

    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.crossThreadDataSize);
    iOpenCL::SPatchDataParameterStream dataParameterStream = {};
    dataParameterStream.DataParameterStreamSize = 4096;
    kernelTokens.tokens.dataParameterStream = &dataParameterStream;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(dataParameterStream.DataParameterStreamSize, kernelDescriptor.kernelAttributes.crossThreadDataSize);
    kernelTokens.tokens.dataParameterStream = nullptr;

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.usesPrivateMemory);
    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.perThreadPrivateMemorySize);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.stateless));
    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.pointerSize);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.bindful));
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.bindless));
    iOpenCL::SPatchAllocateStatelessPrivateSurface privateSurface = {};
    privateSurface.DataParamOffset = 2;
    privateSurface.DataParamSize = 3;
    privateSurface.PerThreadPrivateMemorySize = 5;
    privateSurface.SurfaceStateHeapOffset = 7;
    kernelTokens.tokens.allocateStatelessPrivateSurface = &privateSurface;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.usesPrivateMemory);
    EXPECT_EQ(privateSurface.PerThreadPrivateMemorySize, kernelDescriptor.kernelAttributes.perThreadPrivateMemorySize);
    EXPECT_EQ(privateSurface.DataParamOffset, kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.stateless);
    EXPECT_EQ(privateSurface.DataParamSize, kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.pointerSize);
    EXPECT_EQ(privateSurface.SurfaceStateHeapOffset, kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.bindless));
    kernelTokens.tokens.allocateStatelessPrivateSurface = nullptr;

    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless));
    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.pointerSize);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindful));
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless));
    iOpenCL::SPatchAllocateStatelessConstantMemorySurfaceWithInitialization constantSurface = {};
    constantSurface.DataParamOffset = 2;
    constantSurface.DataParamSize = 3;
    constantSurface.SurfaceStateHeapOffset = 7;
    kernelTokens.tokens.allocateStatelessConstantMemorySurfaceWithInitialization = &constantSurface;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(constantSurface.DataParamOffset, kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless);
    EXPECT_EQ(constantSurface.DataParamSize, kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.pointerSize);
    EXPECT_EQ(constantSurface.SurfaceStateHeapOffset, kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless));
    kernelTokens.tokens.allocateStatelessConstantMemorySurfaceWithInitialization = nullptr;

    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless));
    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.pointerSize);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindful));
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless));
    iOpenCL::SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization globalsSurface = {};
    globalsSurface.DataParamOffset = 2;
    globalsSurface.DataParamSize = 3;
    globalsSurface.SurfaceStateHeapOffset = 7;
    kernelTokens.tokens.allocateStatelessGlobalMemorySurfaceWithInitialization = &globalsSurface;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(globalsSurface.DataParamOffset, kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless);
    EXPECT_EQ(globalsSurface.DataParamSize, kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.pointerSize);
    EXPECT_EQ(globalsSurface.SurfaceStateHeapOffset, kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless));
    kernelTokens.tokens.allocateStatelessGlobalMemorySurfaceWithInitialization = nullptr;

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.usesPrintf);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.stateless));
    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.pointerSize);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.bindful));
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.bindless));
    iOpenCL::SPatchAllocateStatelessPrintfSurface printfSurface = {};
    printfSurface.DataParamOffset = 2;
    printfSurface.DataParamSize = 3;
    printfSurface.SurfaceStateHeapOffset = 7;
    kernelTokens.tokens.allocateStatelessPrintfSurface = &printfSurface;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.usesPrintf);
    EXPECT_EQ(printfSurface.DataParamOffset, kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.stateless);
    EXPECT_EQ(printfSurface.DataParamSize, kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.pointerSize);
    EXPECT_EQ(printfSurface.SurfaceStateHeapOffset, kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.bindless));
    kernelTokens.tokens.allocateStatelessPrintfSurface = nullptr;

    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress.stateless));
    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress.pointerSize);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress.bindful));
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress.bindless));
    iOpenCL::SPatchAllocateStatelessEventPoolSurface eventPoolSurface = {};
    eventPoolSurface.DataParamOffset = 2;
    eventPoolSurface.DataParamSize = 3;
    eventPoolSurface.SurfaceStateHeapOffset = 7;
    kernelTokens.tokens.allocateStatelessEventPoolSurface = &eventPoolSurface;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(eventPoolSurface.DataParamOffset, kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress.stateless);
    EXPECT_EQ(eventPoolSurface.DataParamSize, kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress.pointerSize);
    EXPECT_EQ(eventPoolSurface.SurfaceStateHeapOffset, kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress.bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress.bindless));
    kernelTokens.tokens.allocateStatelessEventPoolSurface = nullptr;

    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress.stateless));
    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress.pointerSize);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress.bindful));
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress.bindless));
    iOpenCL::SPatchAllocateStatelessDefaultDeviceQueueSurface defaultDeviceQueueSurface = {};
    defaultDeviceQueueSurface.DataParamOffset = 2;
    defaultDeviceQueueSurface.DataParamSize = 3;
    defaultDeviceQueueSurface.SurfaceStateHeapOffset = 7;
    kernelTokens.tokens.allocateStatelessDefaultDeviceQueueSurface = &defaultDeviceQueueSurface;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(defaultDeviceQueueSurface.DataParamOffset, kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress.stateless);
    EXPECT_EQ(defaultDeviceQueueSurface.DataParamSize, kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress.pointerSize);
    EXPECT_EQ(defaultDeviceQueueSurface.SurfaceStateHeapOffset, kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress.bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress.bindless));
    kernelTokens.tokens.allocateStatelessDefaultDeviceQueueSurface = nullptr;

    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.perThreadSystemThreadSurfaceSize);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.stateless));
    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.pointerSize);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful));
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindless));
    iOpenCL::SPatchAllocateSystemThreadSurface systemThreadSurface = {};
    systemThreadSurface.Offset = 2;
    systemThreadSurface.PerThreadSystemThreadSurfaceSize = 3;
    kernelTokens.tokens.allocateSystemThreadSurface = &systemThreadSurface;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(systemThreadSurface.PerThreadSystemThreadSurfaceSize, kernelDescriptor.kernelAttributes.perThreadSystemThreadSurfaceSize);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.stateless));
    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.pointerSize);
    EXPECT_EQ(systemThreadSurface.Offset, kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindless));
    kernelTokens.tokens.allocateSystemThreadSurface = nullptr;

    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.usesSyncBuffer);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.stateless));
    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.pointerSize);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.bindful));
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.bindless));
    iOpenCL::SPatchAllocateSyncBuffer syncBuffer = {};
    syncBuffer.DataParamOffset = 2;
    syncBuffer.DataParamSize = 3;
    syncBuffer.SurfaceStateHeapOffset = 7;
    kernelTokens.tokens.allocateSyncBuffer = &syncBuffer;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.usesSyncBuffer);
    EXPECT_EQ(defaultDeviceQueueSurface.DataParamOffset, kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.stateless);
    EXPECT_EQ(defaultDeviceQueueSurface.DataParamSize, kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.pointerSize);
    EXPECT_EQ(defaultDeviceQueueSurface.SurfaceStateHeapOffset, kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.bindless));
    kernelTokens.tokens.allocateSyncBuffer = nullptr;
}

TEST(KernelDescriptorFromPatchtokens, GivenPrintfStringThenPopulatesStringsMapInDescriptor) {
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens;
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader;
    kernelTokens.header = &kernelHeader;
    NEO::KernelDescriptor kernelDescriptor;

    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelMetadata.printfStringsMap.empty());

    std::vector<uint8_t> strTokStream;
    std::string str0{"some_string0"};
    std::string str1{"another_string"};
    std::string str2{"yet_another_string"};
    std::string str3;
    auto string0Off = PatchTokensTestData::pushBackStringToken(str0, 0, strTokStream);
    auto string1Off = PatchTokensTestData::pushBackStringToken(str1, 2, strTokStream);
    auto string2Off = PatchTokensTestData::pushBackStringToken(str2, 1, strTokStream);
    auto string3Off = PatchTokensTestData::pushBackStringToken(str3, 3, strTokStream);

    kernelTokens.tokens.strings.push_back(reinterpret_cast<iOpenCL::SPatchString *>(strTokStream.data() + string0Off));
    kernelTokens.tokens.strings.push_back(reinterpret_cast<iOpenCL::SPatchString *>(strTokStream.data() + string1Off));
    kernelTokens.tokens.strings.push_back(reinterpret_cast<iOpenCL::SPatchString *>(strTokStream.data() + string2Off));
    kernelTokens.tokens.strings.push_back(reinterpret_cast<iOpenCL::SPatchString *>(strTokStream.data() + string3Off));

    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    ASSERT_EQ(4U, kernelDescriptor.kernelMetadata.printfStringsMap.size());
    EXPECT_EQ(str0, kernelDescriptor.kernelMetadata.printfStringsMap[0]);
    EXPECT_EQ(str1, kernelDescriptor.kernelMetadata.printfStringsMap[2]);
    EXPECT_EQ(str2, kernelDescriptor.kernelMetadata.printfStringsMap[1]);
    EXPECT_TRUE(kernelDescriptor.kernelMetadata.printfStringsMap[3].empty());
}

TEST(KernelDescriptorFromPatchtokens, GivenPureStatlessAddressingMdelThenBindfulOffsetIsLeftUndefined) {
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens;
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader;
    kernelTokens.header = &kernelHeader;
    NEO::KernelDescriptor kernelDescriptor;

    iOpenCL::SPatchAllocateStatelessPrintfSurface printfSurface = {};
    printfSurface.DataParamOffset = 2;
    printfSurface.DataParamSize = 3;
    printfSurface.SurfaceStateHeapOffset = 7;
    kernelTokens.tokens.allocateStatelessPrintfSurface = &printfSurface;

    kernelDescriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::Stateless;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(printfSurface.DataParamOffset, kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.stateless);
    EXPECT_EQ(printfSurface.DataParamSize, kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.pointerSize);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.bindful));
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.bindless));
}

TEST(KernelDescriptorFromPatchtokens, GivenhKernelAttributesThenPopulatesStringsMapInDescriptor) {
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens;
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader;
    kernelTokens.header = &kernelHeader;
    NEO::KernelDescriptor kernelDescriptor;

    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelMetadata.kernelLanguageAttributes.empty());
    EXPECT_EQ(0U, kernelDescriptor.kernelMetadata.requiredSubGroupSize);

    iOpenCL::SPatchKernelAttributesInfo kernelAttributesToken;
    kernelAttributesToken.AttributesSize = 0U;
    kernelTokens.tokens.kernelAttributesInfo = &kernelAttributesToken;
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_TRUE(kernelDescriptor.kernelMetadata.kernelLanguageAttributes.empty());
    EXPECT_EQ(0U, kernelDescriptor.kernelMetadata.requiredSubGroupSize);

    std::string attribute = "intel_reqd_sub_group_size(32)";
    kernelAttributesToken.AttributesSize = static_cast<uint32_t>(attribute.size());
    std::vector<uint8_t> tokenStorage;
    tokenStorage.insert(tokenStorage.end(), reinterpret_cast<uint8_t *>(&kernelAttributesToken), reinterpret_cast<uint8_t *>(&kernelAttributesToken + 1));
    tokenStorage.insert(tokenStorage.end(), reinterpret_cast<const uint8_t *>(attribute.c_str()), reinterpret_cast<const uint8_t *>(attribute.c_str() + attribute.length()));

    kernelTokens.tokens.kernelAttributesInfo = reinterpret_cast<iOpenCL::SPatchKernelAttributesInfo *>(tokenStorage.data());
    NEO::populateKernelDescriptor(kernelDescriptor, kernelTokens, 4);
    EXPECT_EQ(attribute, kernelDescriptor.kernelMetadata.kernelLanguageAttributes);
    EXPECT_EQ(32U, kernelDescriptor.kernelMetadata.requiredSubGroupSize);
}

TEST(KernelDescriptorFromPatchtokens, GivenValidKernelWithArgThenMetadataIsProperlyPopulated) {
    PatchTokensTestData::ValidProgramWithKernelAndArg src;
    NEO::KernelDescriptor dst = {};
    NEO::populateKernelDescriptor(dst, src.kernels[0], 4);
    ASSERT_EQ(1U, dst.payloadMappings.explicitArgs.size());
    EXPECT_EQ(NEO::KernelArgMetadata::AccessReadWrite, dst.payloadMappings.explicitArgs[0].getTraits().accessQualifier);
    EXPECT_EQ(NEO::KernelArgMetadata::AddrGlobal, dst.payloadMappings.explicitArgs[0].getTraits().addressQualifier);
    NEO::KernelArgMetadata::TypeQualifiers typeQualifiers = {};
    typeQualifiers.constQual = true;
    EXPECT_EQ(typeQualifiers.packed, dst.payloadMappings.explicitArgs[0].getTraits().typeQualifiers.packed);
    EXPECT_EQ(0U, dst.payloadMappings.explicitArgs[0].getTraits().argByValSize);
    ASSERT_EQ(1U, dst.explicitArgsExtendedMetadata.size());
    EXPECT_STREQ("__global", dst.explicitArgsExtendedMetadata[0].addressQualifier.c_str());
    EXPECT_STREQ("read_write", dst.explicitArgsExtendedMetadata[0].accessQualifier.c_str());
    EXPECT_STREQ("custom_arg", dst.explicitArgsExtendedMetadata[0].argName.c_str());
    EXPECT_STREQ("int*", dst.explicitArgsExtendedMetadata[0].type.c_str());
    EXPECT_STREQ("const", dst.explicitArgsExtendedMetadata[0].typeQualifiers.c_str());
}

TEST(KernelDescriptorFromPatchtokens, GivenValidKernelWithImageArgThenArgAccessQualifierIsPopulatedBasedOnArgInfo) {
    PatchTokensTestData::ValidProgramWithKernelAndArg src;
    iOpenCL::SPatchImageMemoryObjectKernelArgument imageArg = {};
    imageArg.Token = iOpenCL::PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT;
    imageArg.Writeable = false;
    src.kernels[0].tokens.kernelArgs[0].objectArg = &imageArg;
    NEO::KernelDescriptor dst = {};
    NEO::populateKernelDescriptor(dst, src.kernels[0], 4);
    ASSERT_EQ(1U, dst.payloadMappings.explicitArgs.size());
    EXPECT_EQ(NEO::KernelArgMetadata::AccessReadWrite, dst.payloadMappings.explicitArgs[0].getTraits().accessQualifier);
}

TEST(KernelDescriptorFromPatchtokens, GivenValidKernelWithImageArgWhenArgInfoIsMissingThenArgAccessQualifierIsPopulatedBasedOnImageArgWriteableFlag) {
    PatchTokensTestData::ValidProgramWithKernelAndArg src;
    iOpenCL::SPatchImageMemoryObjectKernelArgument imageArg = {};
    imageArg.Token = iOpenCL::PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT;
    src.kernels[0].tokens.kernelArgs[0].objectArg = &imageArg;
    src.kernels[0].tokens.kernelArgs[0].argInfo = nullptr;
    {
        imageArg.Writeable = false;
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, src.kernels[0], 4);
        ASSERT_EQ(1U, dst.payloadMappings.explicitArgs.size());
        EXPECT_EQ(NEO::KernelArgMetadata::AccessReadOnly, dst.payloadMappings.explicitArgs[0].getTraits().accessQualifier);
    }

    {
        imageArg.Writeable = true;
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, src.kernels[0], 4);
        ASSERT_EQ(1U, dst.payloadMappings.explicitArgs.size());
        EXPECT_EQ(NEO::KernelArgMetadata::AccessReadWrite, dst.payloadMappings.explicitArgs[0].getTraits().accessQualifier);
    }
}

TEST(KernelDescriptorFromPatchtokens, GivenValidKernelWithNonDelimitedArgTypeThenUsesArgTypeAsIs) {
    PatchTokensTestData::ValidProgramWithKernelAndArg src;
    src.arg0TypeMutable[4] = '*';
    NEO::KernelDescriptor dst = {};
    NEO::populateKernelDescriptor(dst, src.kernels[0], 4);
    ASSERT_EQ(1U, dst.explicitArgsExtendedMetadata.size());
    EXPECT_STREQ("int**", dst.explicitArgsExtendedMetadata[0].type.c_str());
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithGtpinInfoTokenThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    iOpenCL::SPatchItemHeader gtpinInfo = {};
    kernelTokens.tokens.gtpinInfo = &gtpinInfo;

    NEO::KernelDescriptor dst = {};
    NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
    EXPECT_NE(nullptr, dst.external.igcInfoForGtpin);
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithImageMemoryObjectKernelArgumentThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);

    iOpenCL::SPatchImageMemoryObjectKernelArgument imageArg = {};
    imageArg.Token = iOpenCL::PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT;
    imageArg.ArgumentNumber = 1;
    imageArg.Offset = 0x40;

    kernelTokens.tokens.kernelArgs.resize(2);
    kernelTokens.tokens.kernelArgs[1].objectArg = &imageArg;
    {
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_EQ(1U, dst.kernelAttributes.numArgsToPatch);
        ASSERT_EQ(2U, dst.payloadMappings.explicitArgs.size());
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].is<NEO::ArgDescriptor::ArgTImage>());
        EXPECT_EQ(imageArg.Offset, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescImage>().bindful);
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescImage>().bindless));
        EXPECT_FALSE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isMediaImage);
        EXPECT_FALSE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isMediaBlockImage);
        EXPECT_FALSE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isTransformable);
        ;
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().needsPatch);
    }

    {
        imageArg.Type = iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA;
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isMediaImage);
        EXPECT_FALSE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isMediaBlockImage);
        EXPECT_FALSE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isTransformable);
    }

    {
        imageArg.Type = iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA_BLOCK;
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_FALSE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isMediaImage);
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isMediaBlockImage);
        EXPECT_FALSE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isTransformable);
    }

    {
        imageArg.Transformable = 1;
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_FALSE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isMediaImage);
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isMediaBlockImage);
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isTransformable);
    }
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithImageMemoryObjectKernelArgumentWhenAccessQualifierAlreadPopulatedThenDontOverwrite) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);

    iOpenCL::SPatchImageMemoryObjectKernelArgument imageArg = {};
    imageArg.Token = iOpenCL::PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT;
    imageArg.ArgumentNumber = 1;
    imageArg.Offset = 0x40;
    kernelTokens.tokens.kernelArgs.resize(2);
    kernelTokens.tokens.kernelArgs[1].objectArg = &imageArg;

    NEO::KernelDescriptor dst = {};
    dst.payloadMappings.explicitArgs.resize(2);
    dst.payloadMappings.explicitArgs[1].getTraits().accessQualifier = NEO::KernelArgMetadata::AccessNone;
    NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
    EXPECT_EQ(NEO::KernelArgMetadata::AccessNone, dst.payloadMappings.explicitArgs[1].getTraits().accessQualifier);
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithSamplerKernelArgumentThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);

    iOpenCL::SPatchSamplerKernelArgument samplerArg = {};
    samplerArg.Token = iOpenCL::PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT;
    samplerArg.ArgumentNumber = 1;
    samplerArg.Offset = 0x40;
    samplerArg.Type = iOpenCL::SAMPLER_OBJECT_TEXTURE;
    kernelTokens.tokens.kernelArgs.resize(2);
    kernelTokens.tokens.kernelArgs[1].objectArg = &samplerArg;

    NEO::KernelDescriptor dst = {};
    NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
    EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].is<NEO::ArgDescriptor::ArgTSampler>());
    EXPECT_EQ(samplerArg.Offset, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescSampler>().bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescSampler>().bindless));
    EXPECT_EQ(samplerArg.Type, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescSampler>().samplerType);
    EXPECT_FALSE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isAccelerator);
    EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().needsPatch);
    EXPECT_FALSE(dst.kernelAttributes.flags.usesVme);
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithSamplerKernelArgumentWhenSamplerIsVmeThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);

    iOpenCL::SPatchSamplerKernelArgument samplerArg = {};
    samplerArg.Token = iOpenCL::PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT;
    samplerArg.ArgumentNumber = 1;
    samplerArg.Offset = 0x40;
    kernelTokens.tokens.kernelArgs.resize(2);
    kernelTokens.tokens.kernelArgs[1].objectArg = &samplerArg;

    {
        samplerArg.Type = iOpenCL::SAMPLER_OBJECT_VME;
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].is<NEO::ArgDescriptor::ArgTSampler>());
        EXPECT_EQ(samplerArg.Type, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescSampler>().samplerType);
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isAccelerator);
        EXPECT_TRUE(dst.kernelAttributes.flags.usesVme);
    }

    {
        samplerArg.Type = iOpenCL::SAMPLER_OBJECT_VE;
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].is<NEO::ArgDescriptor::ArgTSampler>());
        EXPECT_EQ(samplerArg.Type, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescSampler>().samplerType);
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isAccelerator);
        EXPECT_FALSE(dst.kernelAttributes.flags.usesVme);
    }

    {
        samplerArg.Type = iOpenCL::SAMPLER_OBJECT_VD;
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].is<NEO::ArgDescriptor::ArgTSampler>());
        EXPECT_EQ(samplerArg.Type, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescSampler>().samplerType);
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isAccelerator);
        EXPECT_FALSE(dst.kernelAttributes.flags.usesVme);
    }
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithGlobalObjectArgThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);

    iOpenCL::SPatchGlobalMemoryObjectKernelArgument globalMemArg = {};
    globalMemArg.Token = iOpenCL::PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT;
    globalMemArg.ArgumentNumber = 1;
    globalMemArg.Offset = 0x40;

    kernelTokens.tokens.kernelArgs.resize(2);
    kernelTokens.tokens.kernelArgs[1].objectArg = &globalMemArg;
    NEO::KernelDescriptor dst = {};
    NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
    EXPECT_EQ(1U, dst.kernelAttributes.numArgsToPatch);
    ASSERT_EQ(2U, dst.payloadMappings.explicitArgs.size());
    EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].is<NEO::ArgDescriptor::ArgTPointer>());
    EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().stateless));
    EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().bindless));
    EXPECT_EQ(0U, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().pointerSize);
    EXPECT_EQ(globalMemArg.Offset, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().bindful);
    EXPECT_EQ(NEO::KernelArgMetadata::AddrGlobal, dst.payloadMappings.explicitArgs[1].getTraits().addressQualifier);

    EXPECT_FALSE(dst.payloadMappings.explicitArgs[0].getExtendedTypeInfo().needsPatch);
    EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().needsPatch);
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithStatelessGlobalMemoryObjectArgThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);

    iOpenCL::SPatchStatelessGlobalMemoryObjectKernelArgument globalMemArg = {};
    globalMemArg.Token = iOpenCL::PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT;
    globalMemArg.ArgumentNumber = 1;
    globalMemArg.DataParamOffset = 2;
    globalMemArg.DataParamSize = 4;
    globalMemArg.SurfaceStateHeapOffset = 128;

    kernelTokens.tokens.kernelArgs.resize(2);
    kernelTokens.tokens.kernelArgs[1].objectArg = &globalMemArg;
    NEO::KernelDescriptor dst = {};
    NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
    EXPECT_EQ(1U, dst.kernelAttributes.numArgsToPatch);
    ASSERT_EQ(2U, dst.payloadMappings.explicitArgs.size());
    EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].is<NEO::ArgDescriptor::ArgTPointer>());
    EXPECT_EQ(globalMemArg.DataParamOffset, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().stateless);
    EXPECT_EQ(globalMemArg.DataParamSize, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().pointerSize);
    EXPECT_EQ(globalMemArg.SurfaceStateHeapOffset, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().bindless));
    EXPECT_EQ(NEO::KernelArgMetadata::AddrGlobal, dst.payloadMappings.explicitArgs[1].getTraits().addressQualifier);
    EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().needsPatch);
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithStatelessConstantMemoryObjectArgThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);

    iOpenCL::SPatchStatelessConstantMemoryObjectKernelArgument constantMemArg = {};
    constantMemArg.Token = iOpenCL::PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT;
    constantMemArg.ArgumentNumber = 1;
    constantMemArg.DataParamOffset = 2;
    constantMemArg.DataParamSize = 4;
    constantMemArg.SurfaceStateHeapOffset = 128;

    kernelTokens.tokens.kernelArgs.resize(2);
    kernelTokens.tokens.kernelArgs[1].objectArg = &constantMemArg;
    NEO::KernelDescriptor dst = {};
    NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
    EXPECT_EQ(1U, dst.kernelAttributes.numArgsToPatch);
    ASSERT_EQ(2U, dst.payloadMappings.explicitArgs.size());
    EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].is<NEO::ArgDescriptor::ArgTPointer>());
    EXPECT_EQ(constantMemArg.DataParamOffset, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().stateless);
    EXPECT_EQ(constantMemArg.DataParamSize, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().pointerSize);
    EXPECT_EQ(constantMemArg.SurfaceStateHeapOffset, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().bindless));
    EXPECT_EQ(NEO::KernelArgMetadata::AddrConstant, dst.payloadMappings.explicitArgs[1].getTraits().addressQualifier);
    EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().needsPatch);
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithStatelessDeviceQueueKernelArgumentThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);

    iOpenCL::SPatchStatelessDeviceQueueKernelArgument deviceQueueArg = {};
    deviceQueueArg.Token = iOpenCL::PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT;
    deviceQueueArg.ArgumentNumber = 1;
    deviceQueueArg.DataParamOffset = 2;
    deviceQueueArg.DataParamSize = 4;
    deviceQueueArg.SurfaceStateHeapOffset = 128;

    kernelTokens.tokens.kernelArgs.resize(2);
    kernelTokens.tokens.kernelArgs[1].objectArg = &deviceQueueArg;
    NEO::KernelDescriptor dst = {};
    NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
    EXPECT_EQ(1U, dst.kernelAttributes.numArgsToPatch);
    ASSERT_EQ(2U, dst.payloadMappings.explicitArgs.size());
    EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].is<NEO::ArgDescriptor::ArgTPointer>());
    EXPECT_EQ(deviceQueueArg.DataParamOffset, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().stateless);
    EXPECT_EQ(deviceQueueArg.DataParamSize, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().pointerSize);
    EXPECT_EQ(deviceQueueArg.SurfaceStateHeapOffset, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescPointer>().bindless));
    EXPECT_EQ(NEO::KernelArgMetadata::AddrGlobal, dst.payloadMappings.explicitArgs[1].getTraits().addressQualifier);
    EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().needsPatch);

    EXPECT_FALSE(dst.payloadMappings.explicitArgs[0].getExtendedTypeInfo().isDeviceQueue);
    EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].getExtendedTypeInfo().isDeviceQueue);
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithByValueArgumentsThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);

    iOpenCL::SPatchDataParameterBuffer paramArg10 = {};
    paramArg10.Token = iOpenCL::PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    paramArg10.ArgumentNumber = 1;
    paramArg10.Type = iOpenCL::DATA_PARAMETER_KERNEL_ARGUMENT;
    paramArg10.Offset = 2;
    paramArg10.DataSize = 3;
    paramArg10.SourceOffset = 5;

    iOpenCL::SPatchDataParameterBuffer paramArg11 = {};
    paramArg11.Token = iOpenCL::PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    paramArg11.ArgumentNumber = 1;
    paramArg11.Type = iOpenCL::DATA_PARAMETER_KERNEL_ARGUMENT;
    paramArg11.Offset = 7;
    paramArg11.DataSize = 11;
    paramArg11.SourceOffset = 13;

    iOpenCL::SPatchDataParameterBuffer paramNonArg = {};
    paramNonArg.Token = iOpenCL::PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    paramNonArg.ArgumentNumber = 2;
    paramNonArg.Type = iOpenCL::DATA_PARAMETER_IMAGE_WIDTH;
    paramNonArg.Offset = 17;
    paramNonArg.DataSize = 19;
    paramNonArg.SourceOffset = 23;

    kernelTokens.tokens.kernelArgs.resize(3);
    kernelTokens.tokens.kernelArgs[1].byValMap.push_back(&paramArg10);
    kernelTokens.tokens.kernelArgs[1].byValMap.push_back(&paramArg11);
    kernelTokens.tokens.kernelArgs[2].byValMap.push_back(&paramNonArg);
    NEO::KernelDescriptor dst = {};
    NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
    EXPECT_EQ(3U, dst.payloadMappings.explicitArgs.size());
    EXPECT_EQ(2U, dst.kernelAttributes.numArgsToPatch);
    EXPECT_TRUE(dst.payloadMappings.explicitArgs[1].is<NEO::ArgDescriptor::ArgTValue>());
    EXPECT_TRUE(dst.payloadMappings.explicitArgs[2].is<NEO::ArgDescriptor::ArgTValue>());
    ASSERT_EQ(2U, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescValue>().elements.size());
    EXPECT_EQ(paramArg10.Offset, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescValue>().elements[0].offset);
    EXPECT_EQ(paramArg10.DataSize, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescValue>().elements[0].size);
    EXPECT_EQ(paramArg10.SourceOffset, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescValue>().elements[0].sourceOffset);
    EXPECT_EQ(paramArg11.Offset, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescValue>().elements[1].offset);
    EXPECT_EQ(paramArg11.DataSize, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescValue>().elements[1].size);
    EXPECT_EQ(paramArg11.SourceOffset, dst.payloadMappings.explicitArgs[1].as<NEO::ArgDescValue>().elements[1].sourceOffset);
    EXPECT_EQ(paramNonArg.Offset, dst.payloadMappings.explicitArgs[2].as<NEO::ArgDescValue>().elements[0].offset);
    EXPECT_EQ(paramNonArg.DataSize, dst.payloadMappings.explicitArgs[2].as<NEO::ArgDescValue>().elements[0].size);
    EXPECT_EQ(paramNonArg.SourceOffset, dst.payloadMappings.explicitArgs[2].as<NEO::ArgDescValue>().elements[0].sourceOffset);

    ASSERT_EQ(2U, dst.kernelMetadata.allByValueKernelArguments.size());
    EXPECT_EQ(1U, dst.kernelMetadata.allByValueKernelArguments[0].argNum);
    EXPECT_EQ(paramArg10.Offset, dst.kernelMetadata.allByValueKernelArguments[0].byValueElement.offset);
    EXPECT_EQ(paramArg10.DataSize, dst.kernelMetadata.allByValueKernelArguments[0].byValueElement.size);
    EXPECT_EQ(paramArg10.SourceOffset, dst.kernelMetadata.allByValueKernelArguments[0].byValueElement.sourceOffset);
    EXPECT_EQ(1U, dst.kernelMetadata.allByValueKernelArguments[0].argNum);
    EXPECT_EQ(paramArg11.Offset, dst.kernelMetadata.allByValueKernelArguments[1].byValueElement.offset);
    EXPECT_EQ(paramArg11.DataSize, dst.kernelMetadata.allByValueKernelArguments[1].byValueElement.size);
    EXPECT_EQ(paramArg11.SourceOffset, dst.kernelMetadata.allByValueKernelArguments[1].byValueElement.sourceOffset);
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithPointerArgumentAndMetadataThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    kernelTokens.tokens.kernelArgs.resize(1);
    kernelTokens.tokens.kernelArgs[0].objectType = NEO::PatchTokenBinary::ArgObjectType::Buffer;
    {
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTPointer>());
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().bufferOffset));
        EXPECT_FALSE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().isPureStateful()));
        EXPECT_FALSE(dst.kernelAttributes.flags.usesImages);
        EXPECT_FALSE(dst.kernelAttributes.flags.usesSamplers);
    }
    {
        iOpenCL::SPatchDataParameterBuffer bufferOffset = {};
        bufferOffset.Offset = 17;

        kernelTokens.tokens.kernelArgs[0].metadata.buffer.bufferOffset = &bufferOffset;
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTPointer>());
        EXPECT_EQ(bufferOffset.Offset, dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().bufferOffset);
        EXPECT_FALSE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().isPureStateful()));
        kernelTokens.tokens.kernelArgs[0].metadata.buffer.bufferOffset = nullptr;
        EXPECT_FALSE(dst.kernelAttributes.flags.usesImages);
        EXPECT_FALSE(dst.kernelAttributes.flags.usesSamplers);
    }
    {
        iOpenCL::SPatchDataParameterBuffer pureStateful = {};
        kernelTokens.tokens.kernelArgs[0].metadata.buffer.pureStateful = &pureStateful;
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTPointer>());
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().bufferOffset));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().isPureStateful()));
        kernelTokens.tokens.kernelArgs[0].metadata.buffer.pureStateful = nullptr;
        EXPECT_FALSE(dst.kernelAttributes.flags.usesImages);
        EXPECT_FALSE(dst.kernelAttributes.flags.usesSamplers);
    }
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithImageArgumentAndMetadataThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    kernelTokens.tokens.kernelArgs.resize(1);
    kernelTokens.tokens.kernelArgs[0].objectType = NEO::PatchTokenBinary::ArgObjectType::Image;
    {
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTImage>());
        auto &metadataPayload = dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescImage>().metadataPayload;
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.imgWidth));
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.imgHeight));
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.imgDepth));
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.channelDataType));
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.channelOrder));
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.arraySize));
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.numSamples));
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.numMipLevels));
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.flatBaseOffset));
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.flatWidth));
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.flatHeight));
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.flatPitch));
        EXPECT_TRUE(dst.kernelAttributes.flags.usesImages);
        EXPECT_FALSE(dst.kernelAttributes.flags.usesSamplers);
    }
    {
        iOpenCL::SPatchDataParameterBuffer imgWidth = {};
        iOpenCL::SPatchDataParameterBuffer imgHeight = {};
        iOpenCL::SPatchDataParameterBuffer imgDepth = {};
        iOpenCL::SPatchDataParameterBuffer channelDataType = {};
        iOpenCL::SPatchDataParameterBuffer channelOrder = {};
        iOpenCL::SPatchDataParameterBuffer arraySize = {};
        iOpenCL::SPatchDataParameterBuffer numSamples = {};
        iOpenCL::SPatchDataParameterBuffer numMipLevels = {};
        iOpenCL::SPatchDataParameterBuffer flatBaseOffset = {};
        iOpenCL::SPatchDataParameterBuffer flatWidth = {};
        iOpenCL::SPatchDataParameterBuffer flatHeight = {};
        iOpenCL::SPatchDataParameterBuffer flatPitch = {};
        imgWidth.Offset = 2;
        imgHeight.Offset = 3;
        imgDepth.Offset = 5;
        channelDataType.Offset = 7;
        channelOrder.Offset = 11;
        arraySize.Offset = 13;
        numSamples.Offset = 17;
        numMipLevels.Offset = 19;
        flatBaseOffset.Offset = 23;
        flatWidth.Offset = 29;
        flatHeight.Offset = 31;
        flatPitch.Offset = 37;

        kernelTokens.tokens.kernelArgs[0].metadata.image.width = &imgWidth;
        kernelTokens.tokens.kernelArgs[0].metadata.image.width = &imgWidth;
        kernelTokens.tokens.kernelArgs[0].metadata.image.height = &imgHeight;
        kernelTokens.tokens.kernelArgs[0].metadata.image.depth = &imgDepth;
        kernelTokens.tokens.kernelArgs[0].metadata.image.channelDataType = &channelDataType;
        kernelTokens.tokens.kernelArgs[0].metadata.image.channelOrder = &channelOrder;
        kernelTokens.tokens.kernelArgs[0].metadata.image.arraySize = &arraySize;
        kernelTokens.tokens.kernelArgs[0].metadata.image.numSamples = &numSamples;
        kernelTokens.tokens.kernelArgs[0].metadata.image.numMipLevels = &numMipLevels;
        kernelTokens.tokens.kernelArgs[0].metadata.image.flatBaseOffset = &flatBaseOffset;
        kernelTokens.tokens.kernelArgs[0].metadata.image.flatWidth = &flatWidth;
        kernelTokens.tokens.kernelArgs[0].metadata.image.flatHeight = &flatHeight;
        kernelTokens.tokens.kernelArgs[0].metadata.image.flatPitch = &flatPitch;

        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTImage>());
        auto &metadataPayload = dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescImage>().metadataPayload;
        EXPECT_EQ(imgWidth.Offset, metadataPayload.imgWidth);
        EXPECT_EQ(imgHeight.Offset, metadataPayload.imgHeight);
        EXPECT_EQ(imgDepth.Offset, metadataPayload.imgDepth);
        EXPECT_EQ(channelDataType.Offset, metadataPayload.channelDataType);
        EXPECT_EQ(channelOrder.Offset, metadataPayload.channelOrder);
        EXPECT_EQ(arraySize.Offset, metadataPayload.arraySize);
        EXPECT_EQ(numSamples.Offset, metadataPayload.numSamples);
        EXPECT_EQ(numMipLevels.Offset, metadataPayload.numMipLevels);
        EXPECT_EQ(flatBaseOffset.Offset, metadataPayload.flatBaseOffset);
        EXPECT_EQ(flatWidth.Offset, metadataPayload.flatWidth);
        EXPECT_EQ(flatHeight.Offset, metadataPayload.flatHeight);
        EXPECT_EQ(flatPitch.Offset, metadataPayload.flatPitch);
        EXPECT_TRUE(dst.kernelAttributes.flags.usesImages);
        EXPECT_FALSE(dst.kernelAttributes.flags.usesSamplers);
    }
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithSamplerArgumentAndMetadataThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    kernelTokens.tokens.kernelArgs.resize(1);
    kernelTokens.tokens.kernelArgs[0].objectType = NEO::PatchTokenBinary::ArgObjectType::Sampler;
    {
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTSampler>());
        auto &metadataPayload = dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescSampler>().metadataPayload;
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.samplerAddressingMode));
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.samplerNormalizedCoords));
        EXPECT_TRUE(NEO::isUndefinedOffset(metadataPayload.samplerSnapWa));
        EXPECT_FALSE(dst.kernelAttributes.flags.usesImages);
        EXPECT_TRUE(dst.kernelAttributes.flags.usesSamplers);
    }
    {
        iOpenCL::SPatchDataParameterBuffer addressingMode = {};
        iOpenCL::SPatchDataParameterBuffer normalizedCoords = {};
        iOpenCL::SPatchDataParameterBuffer snapWa = {};
        addressingMode.Offset = 2;
        normalizedCoords.Offset = 3;
        snapWa.Offset = 5;

        kernelTokens.tokens.kernelArgs[0].metadata.sampler.addressMode = &addressingMode;
        kernelTokens.tokens.kernelArgs[0].metadata.sampler.normalizedCoords = &normalizedCoords;
        kernelTokens.tokens.kernelArgs[0].metadata.sampler.coordinateSnapWaRequired = &snapWa;

        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTSampler>());
        auto &metadataPayload = dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescSampler>().metadataPayload;
        EXPECT_EQ(addressingMode.Offset, metadataPayload.samplerAddressingMode);
        EXPECT_EQ(normalizedCoords.Offset, metadataPayload.samplerNormalizedCoords);
        EXPECT_EQ(snapWa.Offset, metadataPayload.samplerSnapWa);
        EXPECT_FALSE(dst.kernelAttributes.flags.usesImages);
        EXPECT_TRUE(dst.kernelAttributes.flags.usesSamplers);
    }
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithSlmArgumentAndMetadataThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    kernelTokens.tokens.kernelArgs.resize(1);
    {
        kernelTokens.tokens.kernelArgs[0].objectType = NEO::PatchTokenBinary::ArgObjectType::Buffer;
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTPointer>());
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().bufferOffset));
        EXPECT_FALSE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().isPureStateful()));
        EXPECT_FALSE(dst.kernelAttributes.flags.usesImages);
        EXPECT_FALSE(dst.kernelAttributes.flags.usesSamplers);
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().bindful));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().stateless));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().bindless));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().bufferOffset));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().slmOffset));
        EXPECT_EQ(0U, dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().requiredSlmAlignment);
        EXPECT_EQ(0U, dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().pointerSize);
    }
    {
        kernelTokens.tokens.kernelArgs[0].objectType = NEO::PatchTokenBinary::ArgObjectType::Slm;
        iOpenCL::SPatchDataParameterBuffer slmDesc = {};
        slmDesc.Offset = 17;
        slmDesc.SourceOffset = 64;

        kernelTokens.tokens.kernelArgs[0].metadata.slm.token = &slmDesc;
        kernelTokens.tokens.kernelArgs[0].byValMap.push_back(&slmDesc);
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTPointer>());
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().bufferOffset));
        EXPECT_FALSE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().isPureStateful()));
        EXPECT_FALSE(dst.kernelAttributes.flags.usesImages);
        EXPECT_FALSE(dst.kernelAttributes.flags.usesSamplers);
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().bindful));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().stateless));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().bindless));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().bufferOffset));
        EXPECT_EQ(slmDesc.SourceOffset, dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().requiredSlmAlignment);
        EXPECT_EQ(slmDesc.Offset, dst.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>().slmOffset);
    }
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithSamplerArgumentAndMetadataWhenSamplerTypeIsVmeThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    kernelTokens.tokens.kernelArgs.resize(1);
    kernelTokens.tokens.kernelArgs[0].objectType = NEO::PatchTokenBinary::ArgObjectType::Sampler;
    {
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTSampler>());
        EXPECT_TRUE(dst.kernelAttributes.flags.usesSamplers);
        EXPECT_FALSE(dst.payloadMappings.explicitArgs[0].getExtendedTypeInfo().hasVmeExtendedDescriptor);
        EXPECT_TRUE(dst.payloadMappings.explicitArgsExtendedDescriptors.empty());
    }
    {
        kernelTokens.tokens.kernelArgs[0].objectTypeSpecialized = NEO::PatchTokenBinary::ArgObjectTypeSpecialized::Vme;
        iOpenCL::SPatchDataParameterBuffer mbBlockType = {};
        iOpenCL::SPatchDataParameterBuffer subpixelMode = {};
        iOpenCL::SPatchDataParameterBuffer sadAdjustMode = {};
        iOpenCL::SPatchDataParameterBuffer searchPathType = {};
        mbBlockType.Offset = 2;
        subpixelMode.Offset = 3;
        sadAdjustMode.Offset = 5;
        searchPathType.Offset = 7;

        kernelTokens.tokens.kernelArgs[0].metadataSpecialized.vme.mbBlockType = &mbBlockType;
        kernelTokens.tokens.kernelArgs[0].metadataSpecialized.vme.subpixelMode = &subpixelMode;
        kernelTokens.tokens.kernelArgs[0].metadataSpecialized.vme.sadAdjustMode = &sadAdjustMode;
        kernelTokens.tokens.kernelArgs[0].metadataSpecialized.vme.searchPathType = &searchPathType;

        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTSampler>());
        EXPECT_TRUE(dst.kernelAttributes.flags.usesSamplers);
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].getExtendedTypeInfo().hasVmeExtendedDescriptor);
        ASSERT_EQ(1U, dst.payloadMappings.explicitArgsExtendedDescriptors.size());
        auto argVme = reinterpret_cast<NEO::ArgDescVme *>(dst.payloadMappings.explicitArgsExtendedDescriptors[0].get());
        EXPECT_EQ(mbBlockType.Offset, argVme->mbBlockType);
        EXPECT_EQ(subpixelMode.Offset, argVme->subpixelMode);
        EXPECT_EQ(sadAdjustMode.Offset, argVme->sadAdjustMode);
        EXPECT_EQ(searchPathType.Offset, argVme->searchPathType);
    }
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithSamplerArgumentAndMetadataWhenObjectIdIsPresentThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    kernelTokens.tokens.kernelArgs.resize(1);
    kernelTokens.tokens.kernelArgs[0].objectType = NEO::PatchTokenBinary::ArgObjectType::Sampler;
    {
        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTSampler>());
        EXPECT_FALSE(dst.payloadMappings.explicitArgs[0].getExtendedTypeInfo().hasDeviceSideEnqueueExtendedDescriptor);
        EXPECT_TRUE(dst.payloadMappings.explicitArgsExtendedDescriptors.empty());
    }
    {
        iOpenCL::SPatchDataParameterBuffer objectId = {};
        kernelTokens.tokens.kernelArgs[0].objectId = &objectId;
        objectId.Offset = 7;

        NEO::KernelDescriptor dst = {};
        NEO::populateKernelDescriptor(dst, kernelTokens, sizeof(void *));
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTSampler>());
        EXPECT_TRUE(dst.kernelAttributes.flags.usesSamplers);
        EXPECT_TRUE(dst.payloadMappings.explicitArgs[0].getExtendedTypeInfo().hasDeviceSideEnqueueExtendedDescriptor);
        ASSERT_EQ(1U, dst.payloadMappings.explicitArgsExtendedDescriptors.size());
        auto argObjectId = reinterpret_cast<NEO::ArgDescriptorDeviceSideEnqueue *>(dst.payloadMappings.explicitArgsExtendedDescriptors[0].get());
        EXPECT_EQ(objectId.Offset, argObjectId->objectId);
    }
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithInlineVmeThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    NEO::KernelDescriptor dst;
    NEO::populateKernelDescriptor(dst, kernelTokens, 4);
    EXPECT_FALSE(dst.kernelAttributes.flags.usesVme);

    iOpenCL::SPatchItemHeader inlineVme = {};
    kernelTokens.tokens.inlineVmeSamplerInfo = &inlineVme;
    NEO::populateKernelDescriptor(dst, kernelTokens, 4);
    EXPECT_TRUE(dst.kernelAttributes.flags.usesVme);
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithSipDataThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    NEO::KernelDescriptor dst;
    NEO::populateKernelDescriptor(dst, kernelTokens, 4);
    EXPECT_EQ(0U, dst.entryPoints.systemKernel);

    iOpenCL::SPatchStateSIP sip = {};
    sip.SystemKernelOffset = 4096;
    kernelTokens.tokens.stateSip = &sip;
    NEO::populateKernelDescriptor(dst, kernelTokens, 4);
    EXPECT_EQ(sip.SystemKernelOffset, dst.entryPoints.systemKernel);
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithDitpatchMetadataImplicitArgsThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    {
        NEO::KernelDescriptor dst;
        NEO::populateKernelDescriptor(dst, kernelTokens, 4);
        for (uint32_t i = 0; i < 3U; ++i) {
            EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.dispatchTraits.localWorkSize[i])) << i;
            EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.dispatchTraits.localWorkSize2[i])) << i;
            EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.dispatchTraits.globalWorkOffset[i])) << i;
            EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[i])) << i;
            EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.dispatchTraits.globalWorkSize[i])) << i;
            EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.dispatchTraits.numWorkGroups[i])) << i;
        }
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.dispatchTraits.workDim));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.implicitArgs.maxWorkGroupSize));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.implicitArgs.simdSize));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.implicitArgs.deviceSideEnqueueParentEvent));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.implicitArgs.preferredWkgMultiple));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.implicitArgs.privateMemorySize));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.implicitArgs.localMemoryStatelessWindowSize));
        EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.implicitArgs.localMemoryStatelessWindowStartAddres));
    }

    {
        iOpenCL::SPatchDataParameterBuffer localWorkSize[3] = {};
        iOpenCL::SPatchDataParameterBuffer localWorkSize2[3] = {};
        iOpenCL::SPatchDataParameterBuffer enqueuedLocalWorkSize[3] = {};
        iOpenCL::SPatchDataParameterBuffer numWorkGroups[3] = {};
        iOpenCL::SPatchDataParameterBuffer globalWorkOffset[3] = {};
        iOpenCL::SPatchDataParameterBuffer globalWorkSize[3] = {};
        iOpenCL::SPatchDataParameterBuffer maxWorkGroupSize = {};
        iOpenCL::SPatchDataParameterBuffer workDimensions = {};
        iOpenCL::SPatchDataParameterBuffer simdSize = {};
        iOpenCL::SPatchDataParameterBuffer parentEvent = {};
        iOpenCL::SPatchDataParameterBuffer privateMemoryStatelessSize = {};
        iOpenCL::SPatchDataParameterBuffer localMemoryStatelessWindowSize = {};
        iOpenCL::SPatchDataParameterBuffer localMemoryStatelessWindowStartAddress = {};
        iOpenCL::SPatchDataParameterBuffer preferredWorkgroupMultiple = {};
        localWorkSize[0].Offset = 2;
        localWorkSize[1].Offset = 3;
        localWorkSize[2].Offset = 5;
        localWorkSize2[0].Offset = 7;
        localWorkSize2[1].Offset = 11;
        localWorkSize2[2].Offset = 13;
        enqueuedLocalWorkSize[0].Offset = 17;
        enqueuedLocalWorkSize[1].Offset = 19;
        enqueuedLocalWorkSize[2].Offset = 23;
        numWorkGroups[0].Offset = 23;
        numWorkGroups[1].Offset = 29;
        numWorkGroups[2].Offset = 31;
        globalWorkOffset[0].Offset = 37;
        globalWorkOffset[1].Offset = 41;
        globalWorkOffset[2].Offset = 43;
        globalWorkSize[0].Offset = 47;
        globalWorkSize[1].Offset = 53;
        globalWorkSize[2].Offset = 59;
        maxWorkGroupSize.Offset = 61;
        workDimensions.Offset = 67;
        simdSize.Offset = 71;
        parentEvent.Offset = 73;
        privateMemoryStatelessSize.Offset = 79;
        localMemoryStatelessWindowSize.Offset = 83;
        localMemoryStatelessWindowStartAddress.Offset = 89;
        preferredWorkgroupMultiple.Offset = 91;
        for (uint32_t i = 0; i < 3U; ++i) {
            kernelTokens.tokens.crossThreadPayloadArgs.localWorkSize[i] = &localWorkSize[i];
            kernelTokens.tokens.crossThreadPayloadArgs.localWorkSize2[i] = &localWorkSize2[i];
            kernelTokens.tokens.crossThreadPayloadArgs.globalWorkOffset[i] = &globalWorkOffset[i];
            kernelTokens.tokens.crossThreadPayloadArgs.enqueuedLocalWorkSize[i] = &enqueuedLocalWorkSize[i];
            kernelTokens.tokens.crossThreadPayloadArgs.globalWorkSize[i] = &globalWorkSize[i];
            kernelTokens.tokens.crossThreadPayloadArgs.numWorkGroups[i] = &numWorkGroups[i];
        }
        kernelTokens.tokens.crossThreadPayloadArgs.workDimensions = &workDimensions;
        kernelTokens.tokens.crossThreadPayloadArgs.maxWorkGroupSize = &maxWorkGroupSize;
        kernelTokens.tokens.crossThreadPayloadArgs.simdSize = &simdSize;
        kernelTokens.tokens.crossThreadPayloadArgs.parentEvent = &parentEvent;
        kernelTokens.tokens.crossThreadPayloadArgs.preferredWorkgroupMultiple = &preferredWorkgroupMultiple;
        kernelTokens.tokens.crossThreadPayloadArgs.privateMemoryStatelessSize = &privateMemoryStatelessSize;
        kernelTokens.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowSize = &localMemoryStatelessWindowSize;
        kernelTokens.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowStartAddress = &localMemoryStatelessWindowStartAddress;

        NEO::KernelDescriptor dst;
        NEO::populateKernelDescriptor(dst, kernelTokens, 4);
        for (uint32_t i = 0; i < 3U; ++i) {
            EXPECT_NE(0U, dst.payloadMappings.dispatchTraits.localWorkSize[i]) << i;
            EXPECT_NE(0U, dst.payloadMappings.dispatchTraits.localWorkSize2[i]) << i;
            EXPECT_NE(0U, dst.payloadMappings.dispatchTraits.globalWorkOffset[i]) << i;
            EXPECT_NE(0U, dst.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[i]) << i;
            EXPECT_NE(0U, dst.payloadMappings.dispatchTraits.globalWorkSize[i]) << i;
            EXPECT_NE(0U, dst.payloadMappings.dispatchTraits.numWorkGroups[i]) << i;

            EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.localWorkSize[i]->Offset, dst.payloadMappings.dispatchTraits.localWorkSize[i]) << i;
            EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.localWorkSize2[i]->Offset, dst.payloadMappings.dispatchTraits.localWorkSize2[i]) << i;
            EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.globalWorkOffset[i]->Offset, dst.payloadMappings.dispatchTraits.globalWorkOffset[i]) << i;
            EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.enqueuedLocalWorkSize[i]->Offset, dst.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[i]) << i;
            EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.globalWorkSize[i]->Offset, dst.payloadMappings.dispatchTraits.globalWorkSize[i]) << i;
            EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.numWorkGroups[i]->Offset, dst.payloadMappings.dispatchTraits.numWorkGroups[i]) << i;
        }
        EXPECT_NE(0U, dst.payloadMappings.dispatchTraits.workDim);
        EXPECT_NE(0U, dst.payloadMappings.implicitArgs.maxWorkGroupSize);
        EXPECT_NE(0U, dst.payloadMappings.implicitArgs.simdSize);
        EXPECT_NE(0U, dst.payloadMappings.implicitArgs.deviceSideEnqueueParentEvent);
        EXPECT_NE(0U, dst.payloadMappings.implicitArgs.preferredWkgMultiple);
        EXPECT_NE(0U, dst.payloadMappings.implicitArgs.privateMemorySize);
        EXPECT_NE(0U, dst.payloadMappings.implicitArgs.localMemoryStatelessWindowSize);
        EXPECT_NE(0U, dst.payloadMappings.implicitArgs.localMemoryStatelessWindowStartAddres);

        EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.workDimensions->Offset, dst.payloadMappings.dispatchTraits.workDim);
        EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.maxWorkGroupSize->Offset, dst.payloadMappings.implicitArgs.maxWorkGroupSize);
        EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.simdSize->Offset, dst.payloadMappings.implicitArgs.simdSize);
        EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.parentEvent->Offset, dst.payloadMappings.implicitArgs.deviceSideEnqueueParentEvent);
        EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.preferredWorkgroupMultiple->Offset, dst.payloadMappings.implicitArgs.preferredWkgMultiple);
        EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.privateMemoryStatelessSize->Offset, dst.payloadMappings.implicitArgs.privateMemorySize);
        EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowSize->Offset, dst.payloadMappings.implicitArgs.localMemoryStatelessWindowSize);
        EXPECT_EQ(kernelTokens.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowStartAddress->Offset, dst.payloadMappings.implicitArgs.localMemoryStatelessWindowStartAddres);
    }
}

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithChildBlocksMetadataImplicitArgsThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    NEO::KernelDescriptor dst;
    NEO::populateKernelDescriptor(dst, kernelTokens, 4);
    EXPECT_TRUE(dst.kernelMetadata.deviceSideEnqueueChildrenKernelsIdOffset.empty());

    iOpenCL::SPatchDataParameterBuffer childBlocks[2] = {};
    childBlocks[0].ArgumentNumber = 0;
    childBlocks[1].ArgumentNumber = 1;
    childBlocks[0].Offset = 4096;
    childBlocks[1].Offset = 8192;
    kernelTokens.tokens.crossThreadPayloadArgs.childBlockSimdSize.push_back(&childBlocks[0]);
    kernelTokens.tokens.crossThreadPayloadArgs.childBlockSimdSize.push_back(&childBlocks[1]);
    NEO::populateKernelDescriptor(dst, kernelTokens, 4);
    ASSERT_EQ(2U, dst.kernelMetadata.deviceSideEnqueueChildrenKernelsIdOffset.size());
    EXPECT_EQ(childBlocks[0].ArgumentNumber, dst.kernelMetadata.deviceSideEnqueueChildrenKernelsIdOffset[0].first);
    EXPECT_EQ(childBlocks[1].ArgumentNumber, dst.kernelMetadata.deviceSideEnqueueChildrenKernelsIdOffset[1].first);
    EXPECT_EQ(childBlocks[0].Offset, dst.kernelMetadata.deviceSideEnqueueChildrenKernelsIdOffset[0].second);
    EXPECT_EQ(childBlocks[1].Offset, dst.kernelMetadata.deviceSideEnqueueChildrenKernelsIdOffset[1].second);
}
