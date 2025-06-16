/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_arg_descriptor.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/test/common/test_macros/test.h"

TEST(KernelDescriptor, WhenDefaultInitializedThenValuesAreCleared) {
    NEO::KernelDescriptor desc;
    for (auto &element : desc.kernelAttributes.flags.packed) {
        EXPECT_EQ(0U, element);
    }
    EXPECT_EQ(0U, desc.kernelAttributes.slmInlineSize);
    EXPECT_EQ(0U, desc.kernelAttributes.perThreadScratchSize[0]);
    EXPECT_EQ(0U, desc.kernelAttributes.perThreadScratchSize[1]);
    EXPECT_EQ(0U, desc.kernelAttributes.perHwThreadPrivateMemorySize);
    EXPECT_EQ(0U, desc.kernelAttributes.perThreadSystemThreadSurfaceSize);
    EXPECT_EQ(0U, desc.kernelAttributes.requiredWorkgroupSize[0]);
    EXPECT_EQ(0U, desc.kernelAttributes.requiredWorkgroupSize[1]);
    EXPECT_EQ(0U, desc.kernelAttributes.requiredWorkgroupSize[2]);
    EXPECT_EQ(0U, desc.kernelAttributes.crossThreadDataSize);
    EXPECT_EQ(0U, desc.kernelAttributes.perThreadDataSize);
    EXPECT_EQ(0U, desc.kernelAttributes.numArgsToPatch);
    EXPECT_EQ(128U, desc.kernelAttributes.numGrfRequired);
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
    EXPECT_EQ(0U, desc.kernelAttributes.numLocalIdChannels);

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
    EXPECT_EQ(0U, desc.kernelMetadata.compiledSubGroupsNumber);
    EXPECT_EQ(0U, desc.kernelMetadata.requiredSubGroupSize);
    EXPECT_EQ(0U, desc.kernelMetadata.requiredThreadGroupDispatchSize);
    EXPECT_EQ(nullptr, desc.external.debugData.get());
    EXPECT_EQ(nullptr, desc.external.igcInfoForGtpin);
}

TEST(KernelDescriptorAttributesSupportsBuffersBiggerThan4Gb, GivenPureStatelessBufferAddressingThenReturnTrue) {
    NEO::KernelDescriptor desc;
    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::Stateless;
    EXPECT_TRUE(desc.kernelAttributes.supportsBuffersBiggerThan4Gb());
}

TEST(KernelDescriptorAttributesSupportsBuffersBiggerThan4Gb, GivenStatefulBufferAddressingThenReturnFalse) {
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

TEST(KernelDescriptor, GivenBufferOrImageBindlessAddressingWhenIsBindlessAddressingKernelCalledThenTrueIsReturned) {
    NEO::KernelDescriptor desc;
    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::Bindful;
    EXPECT_FALSE(NEO::KernelDescriptor::isBindlessAddressingKernel(desc));

    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindfulAndStateless;
    EXPECT_FALSE(NEO::KernelDescriptor::isBindlessAddressingKernel(desc));

    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::Bindless;
    EXPECT_TRUE(NEO::KernelDescriptor::isBindlessAddressingKernel(desc));

    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    EXPECT_TRUE(NEO::KernelDescriptor::isBindlessAddressingKernel(desc));

    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::AddrNone;
    desc.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;
    desc.kernelAttributes.flags.usesImages = true;
    EXPECT_TRUE(NEO::KernelDescriptor::isBindlessAddressingKernel(desc));
}

TEST(KernelDescriptor, GivenDescriptorWithBindlessArgsWhenInitBindlessOffsetsToSurfaceStateCalledThenMapIsInitializedOnceAndReturnsCorrectSurfaceIndices) {
    NEO::KernelDescriptor desc;

    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    desc.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x40;

    desc.payloadMappings.explicitArgs.push_back(argDescriptor);

    auto argDescriptor2 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor2.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor2.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor2.as<NEO::ArgDescPointer>().bindless = NEO::undefined<NEO::CrossThreadDataOffset>;
    argDescriptor2.as<NEO::ArgDescPointer>().stateless = 0x80;

    desc.payloadMappings.explicitArgs.push_back(argDescriptor2);

    auto argDescriptor3 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTImage);
    argDescriptor3.as<NEO::ArgDescImage>() = NEO::ArgDescImage();
    argDescriptor3.as<NEO::ArgDescImage>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor3.as<NEO::ArgDescImage>().bindless = 0x100;

    desc.payloadMappings.explicitArgs.push_back(argDescriptor3);

    auto argDescriptor4 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTImage);
    argDescriptor4.as<NEO::ArgDescImage>() = NEO::ArgDescImage();
    argDescriptor4.as<NEO::ArgDescImage>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor4.as<NEO::ArgDescImage>().bindless = NEO::undefined<NEO::CrossThreadDataOffset>;

    desc.payloadMappings.explicitArgs.push_back(argDescriptor4);

    argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x80;
    desc.payloadMappings.implicitArgs.globalVariablesSurfaceAddress = argDescriptor.as<NEO::ArgDescPointer>();

    desc.initBindlessOffsetToSurfaceState();
    EXPECT_EQ(3u, desc.bindlessArgsMap.size());

    EXPECT_EQ(0u, desc.bindlessArgsMap[0x40]);
    EXPECT_EQ(1u, desc.bindlessArgsMap[0x100]);
    EXPECT_EQ(2u, desc.bindlessArgsMap[0x80]);

    EXPECT_EQ(0u, desc.getBindlessOffsetToSurfaceState().find(0x40)->second);
    EXPECT_EQ(1u, desc.getBindlessOffsetToSurfaceState().find(0x100)->second);
    EXPECT_EQ(2u, desc.getBindlessOffsetToSurfaceState().find(0x80)->second);

    desc.bindlessArgsMap.clear();
    desc.initBindlessOffsetToSurfaceState();
    EXPECT_EQ(0u, desc.bindlessArgsMap.size());
}

TEST(KernelDescriptor, GivenDescriptorWithBindlessExplicitAndImplicitArgsWhenInitBindlessOffsetsToSurfaceStateCalledThenMapIsInitializedOnceAndReturnsCorrectSurfaceIndices) {
    NEO::KernelDescriptor desc;

    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    desc.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x40;

    desc.payloadMappings.explicitArgs.push_back(argDescriptor);

    auto argDescriptor2 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor2.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor2.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor2.as<NEO::ArgDescPointer>().bindless = NEO::undefined<NEO::CrossThreadDataOffset>;
    argDescriptor2.as<NEO::ArgDescPointer>().stateless = 0x80;

    desc.payloadMappings.explicitArgs.push_back(argDescriptor2);

    auto argDescriptor3 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTImage);
    argDescriptor3.as<NEO::ArgDescImage>() = NEO::ArgDescImage();
    argDescriptor3.as<NEO::ArgDescImage>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor3.as<NEO::ArgDescImage>().bindless = 0x100;

    desc.payloadMappings.explicitArgs.push_back(argDescriptor3);

    auto argDescriptor4 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTImage);
    argDescriptor4.as<NEO::ArgDescImage>() = NEO::ArgDescImage();
    argDescriptor4.as<NEO::ArgDescImage>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor4.as<NEO::ArgDescImage>().bindless = NEO::undefined<NEO::CrossThreadDataOffset>;

    desc.payloadMappings.explicitArgs.push_back(argDescriptor4);

    desc.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless = 0x140;
    desc.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless = 0x180;

    desc.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless = 0x220;
    desc.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless = 0x260;

    desc.initBindlessOffsetToSurfaceState();
    EXPECT_EQ(4u, desc.bindlessArgsMap.size());

    EXPECT_EQ(0u, desc.bindlessArgsMap[0x40]);
    EXPECT_EQ(1u, desc.bindlessArgsMap[0x100]);
    EXPECT_EQ(2u, desc.bindlessArgsMap[0x140]);
    EXPECT_EQ(3u, desc.bindlessArgsMap[0x220]);

    EXPECT_EQ(0u, desc.getBindlessOffsetToSurfaceState().find(0x40)->second);
    EXPECT_EQ(1u, desc.getBindlessOffsetToSurfaceState().find(0x100)->second);
    EXPECT_EQ(2u, desc.getBindlessOffsetToSurfaceState().find(0x140)->second);
    EXPECT_EQ(3u, desc.getBindlessOffsetToSurfaceState().find(0x220)->second);

    desc.bindlessArgsMap.clear();
    desc.initBindlessOffsetToSurfaceState();
    EXPECT_EQ(0u, desc.bindlessArgsMap.size());
}

TEST(KernelDescriptor, GivenDescriptorWithoutStatefulArgsWhenInitBindlessOffsetsToSurfaceStateCalledThenMapOfBindlessOffsetToSurfaceStateIndexIsEmpty) {
    NEO::KernelDescriptor desc;

    desc.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    desc.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().bindless = NEO::undefined<NEO::CrossThreadDataOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().stateless = 0x40;

    desc.payloadMappings.explicitArgs.push_back(argDescriptor);

    auto argDescriptor2 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor2.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor2.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor2.as<NEO::ArgDescPointer>().bindless = NEO::undefined<NEO::CrossThreadDataOffset>;
    argDescriptor2.as<NEO::ArgDescPointer>().stateless = 0x80;

    desc.payloadMappings.explicitArgs.push_back(argDescriptor2);

    NEO::ArgDescValue::Element argValueElement;
    argValueElement.offset = 0x80;
    auto argDescriptor3 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTValue);
    argDescriptor3.as<NEO::ArgDescValue>().elements.push_back(argValueElement);
    desc.payloadMappings.explicitArgs.push_back(argDescriptor3);

    desc.initBindlessOffsetToSurfaceState();
    EXPECT_EQ(0u, desc.bindlessArgsMap.size());
}

TEST(KernelDescriptor, GivenDescriptorWhenGettingPerThreadDataOffsetThenItReturnsCorrectValue) {
    NEO::KernelDescriptor desc{};

    desc.kernelAttributes.crossThreadDataSize = 64u;
    desc.kernelAttributes.inlineDataPayloadSize = 64u;
    EXPECT_EQ(0u, desc.getPerThreadDataOffset());

    // crossThreadData is fully consumed by inlineDataPayload
    desc.kernelAttributes.crossThreadDataSize = 40u;
    desc.kernelAttributes.inlineDataPayloadSize = 64u;
    EXPECT_EQ(0u, desc.getPerThreadDataOffset());

    desc.kernelAttributes.crossThreadDataSize = 128u;
    desc.kernelAttributes.inlineDataPayloadSize = 64u;
    EXPECT_EQ(64u, desc.getPerThreadDataOffset());
}
