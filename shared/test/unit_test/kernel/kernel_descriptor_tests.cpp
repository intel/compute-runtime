/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_arg_descriptor.h"
#include "shared/source/kernel/kernel_descriptor.h"

#include "test.h"

TEST(KernelDescriptor, WhenDefaultInitializedThenValuesAreCleared) {
    NEO::KernelDescriptor desc;
    EXPECT_EQ(0U, desc.kernelAttributes.flags.packed);
    EXPECT_EQ(0U, desc.kernelAttributes.slmInlineSize);
    EXPECT_EQ(0U, desc.kernelAttributes.perThreadScratchSize[0]);
    EXPECT_EQ(0U, desc.kernelAttributes.perThreadScratchSize[1]);
    EXPECT_EQ(0U, desc.kernelAttributes.perThreadPrivateMemorySize);
    EXPECT_EQ(0U, desc.kernelAttributes.perThreadSystemThreadSurfaceSize);
    EXPECT_EQ(0U, desc.kernelAttributes.requiredWorkgroupSize[0]);
    EXPECT_EQ(0U, desc.kernelAttributes.requiredWorkgroupSize[1]);
    EXPECT_EQ(0U, desc.kernelAttributes.requiredWorkgroupSize[2]);
    EXPECT_EQ(0U, desc.kernelAttributes.crossThreadDataSize);
    EXPECT_EQ(0U, desc.kernelAttributes.perThreadDataSize);
    EXPECT_EQ(0U, desc.kernelAttributes.numArgsToPatch);
    EXPECT_EQ(0U, desc.kernelAttributes.numGrfRequired);
    EXPECT_EQ(NEO::KernelDescriptor::BindfulAndStateless, desc.kernelAttributes.bufferAddressingMode);
    EXPECT_EQ(NEO::KernelDescriptor::Bindful, desc.kernelAttributes.imageAddressingMode);
    EXPECT_EQ(NEO::KernelDescriptor::Bindful, desc.kernelAttributes.samplerAddressingMode);
    EXPECT_EQ(0U, desc.kernelAttributes.workgroupWalkOrder[0]);
    EXPECT_EQ(1U, desc.kernelAttributes.workgroupWalkOrder[1]);
    EXPECT_EQ(2U, desc.kernelAttributes.workgroupWalkOrder[2]);
    EXPECT_EQ(0U, desc.kernelAttributes.workgroupDimensionsOrder[0]);
    EXPECT_EQ(1U, desc.kernelAttributes.workgroupDimensionsOrder[1]);
    EXPECT_EQ(2U, desc.kernelAttributes.workgroupDimensionsOrder[2]);
    EXPECT_EQ(0U, desc.kernelAttributes.gpuPointerSize);
    EXPECT_EQ(8U, desc.kernelAttributes.simdSize);
    EXPECT_EQ(32U, desc.kernelAttributes.grfSize);
    EXPECT_EQ(3U, desc.kernelAttributes.numLocalIdChannels);

    EXPECT_EQ(0U, desc.entryPoints.skipPerThreadDataLoad);
    EXPECT_EQ(0U, desc.entryPoints.skipSetFFIDGP);
    EXPECT_EQ(0U, desc.entryPoints.systemKernel);

    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.globalWorkOffset[0]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.globalWorkOffset[1]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.globalWorkOffset[2]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.globalWorkSize[0]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.globalWorkSize[1]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.globalWorkSize[2]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.localWorkSize[0]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.localWorkSize[1]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.localWorkSize[2]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.localWorkSize2[0]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.localWorkSize2[1]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.localWorkSize2[2]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[0]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[1]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[2]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.numWorkGroups[0]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.numWorkGroups[1]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.numWorkGroups[2]);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.dispatchTraits.workDim);

    EXPECT_EQ(NEO::undefined<NEO::SurfaceStateHeapOffset>, desc.payloadMappings.bindingTable.tableOffset);
    EXPECT_EQ(0U, desc.payloadMappings.bindingTable.numEntries);

    EXPECT_EQ(NEO::undefined<NEO::DynamicStateHeapOffset>, desc.payloadMappings.samplerTable.tableOffset);
    EXPECT_EQ(NEO::undefined<NEO::DynamicStateHeapOffset>, desc.payloadMappings.samplerTable.borderColor);
    EXPECT_EQ(0U, desc.payloadMappings.samplerTable.numSamplers);

    EXPECT_EQ(0U, desc.payloadMappings.explicitArgs.size());
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.implicitArgs.privateMemorySize);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.implicitArgs.maxWorkGroupSize);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.implicitArgs.simdSize);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.implicitArgs.deviceSideEnqueueParentEvent);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.implicitArgs.preferredWkgMultiple);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.implicitArgs.localMemoryStatelessWindowSize);
    EXPECT_EQ(NEO::undefined<NEO::CrossThreadDataOffset>, desc.payloadMappings.implicitArgs.localMemoryStatelessWindowStartAddres);

    EXPECT_EQ(0U, desc.payloadMappings.explicitArgsExtendedDescriptors.size());

    EXPECT_TRUE(desc.kernelMetadata.kernelName.empty());
    EXPECT_TRUE(desc.kernelMetadata.kernelLanguageAttributes.empty());
    EXPECT_TRUE(desc.kernelMetadata.printfStringsMap.empty());
    EXPECT_TRUE(desc.kernelMetadata.deviceSideEnqueueChildrenKernelsIdOffset.empty());
    EXPECT_EQ(0U, desc.kernelMetadata.deviceSideEnqueueBlockInterfaceDescriptorOffset);
    EXPECT_TRUE(desc.kernelMetadata.allByValueKernelArguments.empty());
    EXPECT_EQ(0U, desc.kernelMetadata.compiledSubGroupsNumber);
    EXPECT_EQ(0U, desc.kernelMetadata.requiredSubGroupSize);
    EXPECT_EQ(nullptr, desc.external.debugData.get());
    EXPECT_EQ(nullptr, desc.external.igcInfoForGtpin);
}

TEST(KernelDescriptorAttributesSupportsBuffersBiggerThan4Gb, GivenPureStatelessBufferAddressingThenReturnTrue) {
    NEO::KernelDescriptor desc;
    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::Stateless;
    EXPECT_TRUE(desc.kernelAttributes.supportsBuffersBiggerThan4Gb());
}

TEST(KernelDescriptorAttributesSupportsBuffersBiggerThan4Gb, GiveStatefulBufferAddressingThenReturnFalse) {
    NEO::KernelDescriptor desc;
    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::Bindful;
    EXPECT_FALSE(desc.kernelAttributes.supportsBuffersBiggerThan4Gb());
    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindfulAndStateless;
    EXPECT_FALSE(desc.kernelAttributes.supportsBuffersBiggerThan4Gb());
    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::Bindless;
    EXPECT_FALSE(desc.kernelAttributes.supportsBuffersBiggerThan4Gb());
    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    EXPECT_FALSE(desc.kernelAttributes.supportsBuffersBiggerThan4Gb());
}
