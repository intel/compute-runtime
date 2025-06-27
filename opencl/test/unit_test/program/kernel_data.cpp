/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/helpers/string.h"
#include "shared/test/common/helpers/gtest_helpers.h"

#include "opencl/test/unit_test/fixtures/kernel_data_fixture.h"

TEST_F(KernelDataTest, GivenKernelNameWhenBuildingThenProgramIsCorrect) {
    kernelName = "myTestKernel";
    kernelNameSize = (uint32_t)alignUp(strlen(kernelName.c_str()) + 1, sizeof(uint32_t));

    buildAndDecode();
}

TEST_F(KernelDataTest, GivenHeapsWhenBuildingThenProgramIsCorrect) {
    char gshData[8] = "a";
    char dshData[8] = "bb";
    char sshData[8] = "ccc";
    char kernelHeapData[8] = "dddd";

    pGsh = gshData;
    pDsh = dshData;
    pSsh = sshData;
    pKernelHeap = kernelHeapData;

    gshSize = 4;
    dshSize = 4;
    sshSize = 4;
    kernelHeapSize = 4;

    buildAndDecode();
}

TEST_F(KernelDataTest, GivenAllocateLocalSurfaceWhenBuildingThenProgramIsCorrect) {
    iOpenCL::SPatchAllocateLocalSurface allocateLocalSurface;
    allocateLocalSurface.Token = PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE;
    allocateLocalSurface.Size = sizeof(SPatchAllocateLocalSurface);
    allocateLocalSurface.Offset = 0;                     // think this is SSH offset for local memory when we used to have surface state for local memory
    allocateLocalSurface.TotalInlineLocalMemorySize = 4; // 4 bytes of local memory just for test

    pPatchList = &allocateLocalSurface;
    patchListSize = allocateLocalSurface.Size;

    buildAndDecode();

    EXPECT_EQ_VAL(allocateLocalSurface.TotalInlineLocalMemorySize, pKernelInfo->kernelDescriptor.kernelAttributes.slmInlineSize);
}

TEST_F(KernelDataTest, GivenAllocateStatelessConstantMemoryWithInitWhenBuildingThenProgramIsCorrect) {
    iOpenCL::SPatchAllocateStatelessConstantMemorySurfaceWithInitialization allocateStatelessConstantMemoryWithInit;
    allocateStatelessConstantMemoryWithInit.Token = PATCH_TOKEN_ALLOCATE_STATELESS_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION;
    allocateStatelessConstantMemoryWithInit.Size = sizeof(SPatchAllocateStatelessConstantMemorySurfaceWithInitialization);

    allocateStatelessConstantMemoryWithInit.ConstantBufferIndex = 0;
    allocateStatelessConstantMemoryWithInit.SurfaceStateHeapOffset = 0xddu;

    pPatchList = &allocateStatelessConstantMemoryWithInit;
    patchListSize = allocateStatelessConstantMemoryWithInit.Size;

    buildAndDecode();

    EXPECT_EQ_VAL(0xddu, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindful);
}

TEST_F(KernelDataTest, GivenAllocateStatelessGlobalMemoryWithInitWhenBuildingThenProgramIsCorrect) {
    iOpenCL::SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization allocateStatelessGlobalMemoryWithInit;
    allocateStatelessGlobalMemoryWithInit.Token = PATCH_TOKEN_ALLOCATE_STATELESS_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION;
    allocateStatelessGlobalMemoryWithInit.Size = sizeof(SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization);

    allocateStatelessGlobalMemoryWithInit.GlobalBufferIndex = 0;
    allocateStatelessGlobalMemoryWithInit.SurfaceStateHeapOffset = 0xddu;

    pPatchList = &allocateStatelessGlobalMemoryWithInit;
    patchListSize = allocateStatelessGlobalMemoryWithInit.Size;

    buildAndDecode();

    EXPECT_EQ_VAL(0xddu, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindful);
}

TEST_F(KernelDataTest, GivenPrintfStringWhenBuildingThenProgramIsCorrect) {
    char stringValue[] = "%d\n";
    size_t strSize = strlen(stringValue) + 1;

    iOpenCL::SPatchString printfString;
    printfString.Token = PATCH_TOKEN_STRING;
    printfString.Size = static_cast<uint32_t>(sizeof(SPatchString) + strSize);
    printfString.Index = 0;
    printfString.StringSize = static_cast<uint32_t>(strSize);

    iOpenCL::SPatchString emptyString;
    emptyString.Token = PATCH_TOKEN_STRING;
    emptyString.Size = static_cast<uint32_t>(sizeof(SPatchString));
    emptyString.Index = 1;
    emptyString.StringSize = 0;

    cl_char *pPrintfString = new cl_char[printfString.Size + emptyString.Size];

    memcpy_s(pPrintfString, sizeof(SPatchString), &printfString, sizeof(SPatchString));
    memcpy_s((cl_char *)pPrintfString + sizeof(printfString), strSize, stringValue, strSize);

    memcpy_s((cl_char *)pPrintfString + printfString.Size, emptyString.Size, &emptyString, emptyString.Size);

    pPatchList = (void *)pPrintfString;
    patchListSize = printfString.Size + emptyString.Size;

    buildAndDecode();

    EXPECT_EQ_VAL(0, strcmp(stringValue, pKernelInfo->kernelDescriptor.kernelMetadata.printfStringsMap.find(0)->second.c_str()));
    delete[] pPrintfString;
}

TEST_F(KernelDataTest, GivenMediaVfeStateWhenBuildingThenProgramIsCorrect) {
    iOpenCL::SPatchMediaVFEState mediaVfeState;
    mediaVfeState.Token = PATCH_TOKEN_MEDIA_VFE_STATE;
    mediaVfeState.Size = sizeof(SPatchMediaVFEState);
    mediaVfeState.PerThreadScratchSpace = 1; // lets say 1KB of perThreadScratchSpace
    mediaVfeState.ScratchSpaceOffset = 0;

    pPatchList = &mediaVfeState;
    patchListSize = mediaVfeState.Size;

    buildAndDecode();

    EXPECT_EQ_VAL(mediaVfeState.PerThreadScratchSpace, pKernelInfo->kernelDescriptor.kernelAttributes.perThreadScratchSize[0]);
}

TEST_F(KernelDataTest, WhenMediaVfeStateSlot1TokenIsParsedThenCorrectValuesAreSet) {
    iOpenCL::SPatchMediaVFEState mediaVfeState;
    mediaVfeState.Token = PATCH_TOKEN_MEDIA_VFE_STATE_SLOT1;
    mediaVfeState.Size = sizeof(SPatchMediaVFEState);
    mediaVfeState.PerThreadScratchSpace = 1;
    mediaVfeState.ScratchSpaceOffset = 0;

    pPatchList = &mediaVfeState;
    patchListSize = mediaVfeState.Size;

    buildAndDecode();

    EXPECT_EQ_VAL(mediaVfeState.PerThreadScratchSpace, pKernelInfo->kernelDescriptor.kernelAttributes.perThreadScratchSize[1]);
}

TEST_F(KernelDataTest, GivenSyncBufferTokenWhenParsingProgramThenTokenIsFound) {
    SPatchAllocateSyncBuffer token;
    token.Token = PATCH_TOKEN_ALLOCATE_SYNC_BUFFER;
    token.Size = static_cast<uint32_t>(sizeof(SPatchAllocateSyncBuffer));
    token.SurfaceStateHeapOffset = 32;
    token.DataParamOffset = 1024;
    token.DataParamSize = 2;

    pPatchList = &token;
    patchListSize = token.Size;

    buildAndDecode();

    EXPECT_TRUE(pKernelInfo->kernelDescriptor.kernelAttributes.flags.usesSyncBuffer);
    EXPECT_EQ(token.SurfaceStateHeapOffset, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.bindful);
    EXPECT_EQ(token.DataParamOffset, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.stateless);
    EXPECT_EQ(token.DataParamSize, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.pointerSize);
}

TEST_F(KernelDataTest, GivenSamplerArgumentWhenBuildingThenProgramIsCorrect) {
    iOpenCL::SPatchSamplerKernelArgument samplerData;
    samplerData.Token = PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT;
    samplerData.ArgumentNumber = 3;
    samplerData.Offset = 0x40;
    samplerData.Type = iOpenCL::SAMPLER_OBJECT_TEXTURE;
    samplerData.Size = sizeof(samplerData);

    pPatchList = &samplerData;
    patchListSize = samplerData.Size;

    buildAndDecode();

    EXPECT_TRUE(pKernelInfo->getArgDescriptorAt(3).is<ArgDescriptor::argTSampler>());
    EXPECT_EQ_VAL(samplerData.Offset, pKernelInfo->getArgDescriptorAt(3).as<ArgDescSampler>().bindful);
}

TEST_F(KernelDataTest, GivenAcceleratorArgumentWhenBuildingThenProgramIsCorrect) {
    iOpenCL::SPatchSamplerKernelArgument samplerData;
    samplerData.Token = PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT;
    samplerData.ArgumentNumber = 3;
    samplerData.Offset = 0x40;
    samplerData.Type = iOpenCL::SAMPLER_OBJECT_VME;
    samplerData.Size = sizeof(samplerData);

    pPatchList = &samplerData;
    patchListSize = samplerData.Size;

    buildAndDecode();

    EXPECT_TRUE(pKernelInfo->getArgDescriptorAt(3).is<ArgDescriptor::argTSampler>());
    EXPECT_TRUE(pKernelInfo->getArgDescriptorAt(3).getExtendedTypeInfo().isAccelerator);
    EXPECT_EQ_VAL(samplerData.Offset, pKernelInfo->getArgDescriptorAt(3).as<ArgDescSampler>().bindful);
}

TEST_F(KernelDataTest, GivenBindingTableStateWhenBuildingThenProgramIsCorrect) {
    iOpenCL::SPatchBindingTableState bindingTableState;
    bindingTableState.Token = PATCH_TOKEN_BINDING_TABLE_STATE;
    bindingTableState.Size = sizeof(SPatchBindingTableState);
    bindingTableState.Count = 0xaa;
    bindingTableState.Offset = 0xbb;
    bindingTableState.SurfaceStateOffset = 0xcc;

    pPatchList = &bindingTableState;
    patchListSize = bindingTableState.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(bindingTableState.Count, pKernelInfo->kernelDescriptor.payloadMappings.bindingTable.numEntries);
    EXPECT_EQ_CONST(bindingTableState.Offset, pKernelInfo->kernelDescriptor.payloadMappings.bindingTable.tableOffset);
}

TEST_F(KernelDataTest, GivenDataParameterStreamWhenBuildingThenProgramIsCorrect) {
    iOpenCL::SPatchDataParameterStream dataParameterStream;
    dataParameterStream.Token = PATCH_TOKEN_DATA_PARAMETER_STREAM;
    dataParameterStream.Size = sizeof(SPatchDataParameterStream);
    dataParameterStream.DataParameterStreamSize = 64;

    pPatchList = &dataParameterStream;
    patchListSize = dataParameterStream.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(dataParameterStream.DataParameterStreamSize, pKernelInfo->kernelDescriptor.kernelAttributes.crossThreadDataSize);
}

TEST_F(KernelDataTest, GivenExecutionEnvironmentNoReqdWorkGroupSizeWhenBuildingThenProgramIsCorrect) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);
    executionEnvironment.RequiredWorkGroupSizeX = 0;
    executionEnvironment.RequiredWorkGroupSizeY = 0;
    executionEnvironment.RequiredWorkGroupSizeZ = 0;
    executionEnvironment.LargestCompiledSIMDSize = 32;
    executionEnvironment.CompiledSubGroupsNumber = 0xaa;
    executionEnvironment.HasBarriers = false;
    executionEnvironment.DisableMidThreadPreemption = true;
    executionEnvironment.MayAccessUndeclaredResource = false;
    executionEnvironment.UsesFencesForReadWriteImages = false;
    executionEnvironment.UsesStatelessSpillFill = false;
    executionEnvironment.IsCoherent = true;
    executionEnvironment.IsInitializer = false;
    executionEnvironment.IsFinalizer = false;
    executionEnvironment.SubgroupIndependentForwardProgressRequired = false;
    executionEnvironment.CompiledForGreaterThan4GBBuffers = false;
    executionEnvironment.IndirectStatelessCount = 0;

    pPatchList = &executionEnvironment;
    patchListSize = executionEnvironment.Size;

    buildAndDecode();

    EXPECT_EQ_VAL(0, pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0]);
    EXPECT_EQ_VAL(0, pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1]);
    EXPECT_EQ_VAL(0, pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2]);
    EXPECT_FALSE(pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess);
}

TEST_F(KernelDataTest, GivenExecutionEnvironmentWhenBuildingThenProgramIsCorrect) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);
    executionEnvironment.RequiredWorkGroupSizeX = 32;
    executionEnvironment.RequiredWorkGroupSizeY = 16;
    executionEnvironment.RequiredWorkGroupSizeZ = 8;
    executionEnvironment.LargestCompiledSIMDSize = 32;
    executionEnvironment.CompiledSubGroupsNumber = 0xaa;
    executionEnvironment.HasBarriers = false;
    executionEnvironment.DisableMidThreadPreemption = true;
    executionEnvironment.MayAccessUndeclaredResource = false;
    executionEnvironment.UsesFencesForReadWriteImages = false;
    executionEnvironment.UsesStatelessSpillFill = false;
    executionEnvironment.IsCoherent = true;
    executionEnvironment.IsInitializer = false;
    executionEnvironment.IsFinalizer = false;
    executionEnvironment.SubgroupIndependentForwardProgressRequired = false;
    executionEnvironment.CompiledForGreaterThan4GBBuffers = false;
    executionEnvironment.IndirectStatelessCount = 1;

    pPatchList = &executionEnvironment;
    patchListSize = executionEnvironment.Size;

    buildAndDecode();

    EXPECT_EQ(32u, pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0]);
    EXPECT_EQ(16u, pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1]);
    EXPECT_EQ(8u, pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2]);
    EXPECT_TRUE(pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess);
    EXPECT_EQ(KernelDescriptor::BindfulAndStateless, pKernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode);
}

TEST_F(KernelDataTest, GivenExecutionEnvironmentCompiledForGreaterThan4gbBuffersWhenBuildingThenProgramIsCorrect) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);
    executionEnvironment.RequiredWorkGroupSizeX = 32;
    executionEnvironment.RequiredWorkGroupSizeY = 16;
    executionEnvironment.RequiredWorkGroupSizeZ = 8;
    executionEnvironment.LargestCompiledSIMDSize = 32;
    executionEnvironment.CompiledSubGroupsNumber = 0xaa;
    executionEnvironment.HasBarriers = false;
    executionEnvironment.DisableMidThreadPreemption = true;
    executionEnvironment.MayAccessUndeclaredResource = false;
    executionEnvironment.UsesFencesForReadWriteImages = false;
    executionEnvironment.UsesStatelessSpillFill = false;
    executionEnvironment.IsCoherent = true;
    executionEnvironment.IsInitializer = false;
    executionEnvironment.IsFinalizer = false;
    executionEnvironment.SubgroupIndependentForwardProgressRequired = false;
    executionEnvironment.CompiledForGreaterThan4GBBuffers = true;

    pPatchList = &executionEnvironment;
    patchListSize = executionEnvironment.Size;

    buildAndDecode();

    EXPECT_EQ(KernelDescriptor::Stateless, pKernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode);
}

TEST_F(KernelDataTest, WhenDecodingExecutionEnvironmentTokenThenWalkOrderIsForcedToXMajor) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);

    pPatchList = &executionEnvironment;
    patchListSize = executionEnvironment.Size;

    buildAndDecode();

    const uint8_t expectedWalkOrder[3] = {0, 1, 2};
    const uint8_t expectedDimsIds[3] = {0, 1, 2};
    EXPECT_EQ(0, memcmp(expectedWalkOrder, pKernelInfo->kernelDescriptor.kernelAttributes.workgroupWalkOrder, sizeof(expectedWalkOrder)));
    EXPECT_EQ(0, memcmp(expectedDimsIds, pKernelInfo->kernelDescriptor.kernelAttributes.workgroupDimensionsOrder, sizeof(expectedDimsIds)));
    EXPECT_FALSE(pKernelInfo->kernelDescriptor.kernelAttributes.flags.requiresWorkgroupWalkOrder);
}

TEST_F(KernelDataTest, whenWorkgroupOrderIsSpecifiedViaPatchTokenThenProperWorkGroupOrderIsParsed) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);

    // dim0 : [0 : 1]; dim1 : [2 : 3]; dim2 : [4 : 5]
    executionEnvironment.WorkgroupWalkOrderDims = 1 | (2 << 2);

    pPatchList = &executionEnvironment;
    patchListSize = executionEnvironment.Size;

    buildAndDecode();
    uint8_t expectedWalkOrder[3] = {1, 2, 0};
    uint8_t expectedDimsIds[3] = {2, 0, 1};
    EXPECT_EQ(0, memcmp(expectedWalkOrder, pKernelInfo->kernelDescriptor.kernelAttributes.workgroupWalkOrder, sizeof(expectedWalkOrder)));
    EXPECT_EQ(0, memcmp(expectedDimsIds, pKernelInfo->kernelDescriptor.kernelAttributes.workgroupDimensionsOrder, sizeof(expectedDimsIds)));
    EXPECT_TRUE(pKernelInfo->kernelDescriptor.kernelAttributes.flags.requiresWorkgroupWalkOrder);
}

TEST_F(KernelDataTest, whenWorkgroupOrderIsSpecifiedViaPatchToken2ThenProperWorkGroupOrderIsParsed) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);

    // dim0 : [0 : 1]; dim1 : [2 : 3]; dim2 : [4 : 5]
    executionEnvironment.WorkgroupWalkOrderDims = 2 | (1 << 4);

    pPatchList = &executionEnvironment;
    patchListSize = executionEnvironment.Size;

    buildAndDecode();

    uint8_t expectedWalkOrder[3] = {2, 0, 1};
    uint8_t expectedDimsIds[3] = {1, 2, 0};
    EXPECT_EQ(0, memcmp(expectedWalkOrder, pKernelInfo->kernelDescriptor.kernelAttributes.workgroupWalkOrder, sizeof(expectedWalkOrder)));
    EXPECT_EQ(0, memcmp(expectedDimsIds, pKernelInfo->kernelDescriptor.kernelAttributes.workgroupDimensionsOrder, sizeof(expectedDimsIds)));
    EXPECT_TRUE(pKernelInfo->kernelDescriptor.kernelAttributes.flags.requiresWorkgroupWalkOrder);
}

// Test all the different data parameters with the same "made up" data
class DataParameterTest : public KernelDataTest, public testing::WithParamInterface<uint32_t> {};

TEST_P(DataParameterTest, GivenTokenTypeWhenBuildingThenProgramIsCorrect) {
    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = GetParam();
    dataParameterToken.ArgumentNumber = 1;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;
    dataParameterToken.Offset = 0;
    dataParameterToken.SourceOffset = 8;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    if (pKernelInfo->kernelDescriptor.payloadMappings.explicitArgs.size() > 0) {
        EXPECT_EQ(dataParameterToken.ArgumentNumber + 1, pKernelInfo->kernelDescriptor.payloadMappings.explicitArgs.size());
        const auto &arg = pKernelInfo->getArgDescriptorAt(dataParameterToken.ArgumentNumber);
        if (arg.is<ArgDescriptor::argTPointer>()) {
            const auto &argAsPtr = arg.as<ArgDescPointer>();
            EXPECT_EQ(GetParam() == DATA_PARAMETER_BUFFER_STATEFUL, argAsPtr.isPureStateful());
        }
    }
}

// note that we start at '2' because we test kernel arg tokens elsewhere
INSTANTIATE_TEST_SUITE_P(DataParameterTests,
                         DataParameterTest,
                         testing::Range(2u, static_cast<uint32_t>(NUM_DATA_PARAMETER_TOKENS)));

class KernelDataParameterTest : public KernelDataTest {};
TEST_F(KernelDataParameterTest, GivenDataParameterBufferOffsetWhenBuildingThenProgramIsCorrect) {
    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_BUFFER_OFFSET;
    dataParameterToken.ArgumentNumber = 1;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;
    dataParameterToken.Offset = 128;
    dataParameterToken.SourceOffset = 8;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->kernelDescriptor.payloadMappings.explicitArgs.size());
    EXPECT_EQ_VAL(pKernelInfo->getArgDescriptorAt(1).as<ArgDescPointer>().bufferOffset, dataParameterToken.Offset)
}

TEST_F(KernelDataParameterTest, givenDataParameterBufferStatefulWhenDecodingThenSetArgAsPureStateful) {
    SPatchDataParameterBuffer dataParameterToken = {};
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_BUFFER_STATEFUL;
    dataParameterToken.ArgumentNumber = 1;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());
    EXPECT_TRUE(pKernelInfo->getArgDescriptorAt(1).as<ArgDescPointer>().isPureStateful());
}

TEST_F(KernelDataTest, GivenDataParameterSumOfLocalMemoryObjectArgumentSizesWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 16;
    uint32_t offsetCrossThread = 4;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetCrossThread;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());
    const auto &argAsPtr = pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescPointer>();
    EXPECT_EQ(alignment, argAsPtr.requiredSlmAlignment);
    ASSERT_EQ(offsetCrossThread, argAsPtr.slmOffset);
}

TEST_F(KernelDataTest, GivenDataParameterImageWidthWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 16;
    uint32_t offsetImgWidth = 4;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_IMAGE_WIDTH;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetImgWidth;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());
    EXPECT_EQ(offsetImgWidth, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescImage>().metadataPayload.imgWidth);
}

TEST_F(KernelDataTest, GivenDataParameterImageHeightWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 16;
    uint32_t offsetImgHeight = 8;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_IMAGE_HEIGHT;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetImgHeight;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(offsetImgHeight, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescImage>().metadataPayload.imgHeight);
}

TEST_F(KernelDataTest, GivenDataParameterImageDepthWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 16;
    uint32_t offsetImgDepth = 12;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_IMAGE_DEPTH;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetImgDepth;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(offsetImgDepth, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescImage>().metadataPayload.imgDepth);
}

TEST_F(KernelDataTest, GivenDataParameterImageNumSamplersWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 16;
    uint32_t offsetNumSamples = 60;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_IMAGE_NUM_SAMPLES;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetNumSamples;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(offsetNumSamples, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescImage>().metadataPayload.numSamples);
}

TEST_F(KernelDataTest, GivenDataParameterImageNumMipLevelsWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 16;
    uint32_t offsetNumMipLevels = 60;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_IMAGE_NUM_MIP_LEVELS;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetNumMipLevels;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(offsetNumMipLevels, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescImage>().metadataPayload.numMipLevels);
}

TEST_F(KernelDataTest, givenFlatImageDataParamTokenWhenDecodingThenSetAllOffsets) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 16;

    auto testToken = [&](iOpenCL::DATA_PARAMETER_TOKEN token, uint32_t offsetToken) {
        {
            // reset program
            if (pKernelData) {
                alignedFree(pKernelData);
            }
            program = std::make_unique<MockProgram>(pContext, false, toClDeviceVector(*pContext->getDevice(0)));
        }

        SPatchDataParameterBuffer dataParameterToken;
        dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
        dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
        dataParameterToken.Type = token;
        dataParameterToken.ArgumentNumber = argumentNumber;
        dataParameterToken.Offset = offsetToken;
        dataParameterToken.DataSize = sizeof(uint32_t);
        dataParameterToken.SourceOffset = alignment;
        dataParameterToken.LocationIndex = 0x0;
        dataParameterToken.LocationIndex2 = 0x0;

        pPatchList = &dataParameterToken;
        patchListSize = dataParameterToken.Size;

        buildAndDecode();

        ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());
    };

    testToken(iOpenCL::DATA_PARAMETER_TOKEN::DATA_PARAMETER_FLAT_IMAGE_BASEOFFSET, 10u);
    EXPECT_EQ(10u, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescImage>().metadataPayload.flatBaseOffset);

    testToken(iOpenCL::DATA_PARAMETER_TOKEN::DATA_PARAMETER_FLAT_IMAGE_WIDTH, 14u);
    EXPECT_EQ(14u, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescImage>().metadataPayload.flatWidth);

    testToken(iOpenCL::DATA_PARAMETER_TOKEN::DATA_PARAMETER_FLAT_IMAGE_HEIGHT, 16u);
    EXPECT_EQ(16u, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescImage>().metadataPayload.flatHeight);

    testToken(iOpenCL::DATA_PARAMETER_TOKEN::DATA_PARAMETER_FLAT_IMAGE_PITCH, 18u);
    EXPECT_EQ(18u, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescImage>().metadataPayload.flatPitch);
}

TEST_F(KernelDataTest, GivenDataParameterImageDataTypeWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 16;
    uint32_t offsetChannelDataType = 52;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetChannelDataType;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(offsetChannelDataType, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescImage>().metadataPayload.channelDataType);
}

TEST_F(KernelDataTest, GivenDataParameterImageChannelOrderWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 16;
    uint32_t offsetChannelOrder = 56;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_IMAGE_CHANNEL_ORDER;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetChannelOrder;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(offsetChannelOrder, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescImage>().metadataPayload.channelOrder);
}

TEST_F(KernelDataTest, GivenDataParameterImageArraySizeWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 16;
    uint32_t offsetImageArraySize = 60;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_IMAGE_ARRAY_SIZE;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetImageArraySize;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(offsetImageArraySize, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescImage>().metadataPayload.arraySize);
}

TEST_F(KernelDataTest, GivenDataParameterWorkDimensionsWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 16;
    uint32_t offsetWorkDim = 12;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_WORK_DIMENSIONS;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetWorkDim;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    EXPECT_EQ(0U, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(offsetWorkDim, pKernelInfo->kernelDescriptor.payloadMappings.dispatchTraits.workDim);
}

TEST_F(KernelDataTest, GivenDataParameterSimdSizeWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 17;
    uint32_t alignment = 16;
    uint32_t offsetSimdSize = 16;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_SIMD_SIZE;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetSimdSize;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    EXPECT_EQ(0u, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(offsetSimdSize, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.simdSize);
}

TEST_F(KernelDataTest, GivenParameterPrivateMemoryStatelessSizeWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 17;
    uint32_t alignment = 16;
    uint32_t offset = 16;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_PRIVATE_MEMORY_STATELESS_SIZE;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offset;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    EXPECT_EQ(0u, pKernelInfo->getExplicitArgs().size());
}

TEST_F(KernelDataTest, GivenDataParameterLocalMemoryStatelessWindowSizeWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 17;
    uint32_t alignment = 16;
    uint32_t offset = 16;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_SIZE;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offset;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    EXPECT_EQ(0u, pKernelInfo->getExplicitArgs().size());
}

TEST_F(KernelDataTest, GivenDataParameterLocalMemoryStatelessWindowStartAddressWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 17;
    uint32_t alignment = 16;
    uint32_t offset = 16;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_START_ADDRESS;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offset;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    EXPECT_EQ(0u, pKernelInfo->getExplicitArgs().size());
}

TEST_F(KernelDataTest, GivenDataParameterNumWorkGroupsWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 4;
    uint32_t offsetNumWorkGroups[3] = {0, 4, 8};

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_NUM_WORK_GROUPS;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetNumWorkGroups[argumentNumber];
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = argumentNumber * alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    EXPECT_EQ(0U, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(offsetNumWorkGroups[argumentNumber], pKernelInfo->kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[argumentNumber]);
}

TEST_F(KernelDataTest, GivenDataParameterMaxWorkgroupSizeWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 4;
    uint32_t offsetMaxWorkGroupSize = 4;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_MAX_WORKGROUP_SIZE;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetMaxWorkGroupSize;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    EXPECT_EQ(0U, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(offsetMaxWorkGroupSize, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.maxWorkGroupSize);
}

TEST_F(KernelDataTest, GivenDataParameterSamplerAddressModeWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 0;
    uint32_t dataOffset = 20;
    uint32_t dataSize = sizeof(uint32_t);

    SPatchDataParameterBuffer dataParameterToken;

    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_SAMPLER_ADDRESS_MODE;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = dataOffset;
    dataParameterToken.DataSize = dataSize;
    dataParameterToken.SourceOffset = 0;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(1U, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(dataOffset, pKernelInfo->getArgDescriptorAt(0).as<ArgDescSampler>().metadataPayload.samplerAddressingMode);
}

TEST_F(KernelDataTest, GivenDataParameterSamplerCoordinateSnapWaIsRequiredThenKernelInfoIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t dataOffset = 20;
    uint32_t dataSize = sizeof(uint32_t);

    SPatchDataParameterBuffer dataParameterToken;

    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = dataOffset;
    dataParameterToken.DataSize = dataSize;
    dataParameterToken.SourceOffset = 0;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(dataOffset, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescSampler>().metadataPayload.samplerSnapWa);
}

TEST_F(KernelDataTest, GivenDataParameterSamplerNormalizedCoordsThenKernelInfoIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t dataOffset = 20;
    uint32_t dataSize = sizeof(uint32_t);

    SPatchDataParameterBuffer dataParameterToken;

    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = dataOffset;
    dataParameterToken.DataSize = dataSize;
    dataParameterToken.SourceOffset = 0;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());

    EXPECT_EQ(dataOffset, pKernelInfo->getArgDescriptorAt(argumentNumber).as<ArgDescSampler>().metadataPayload.samplerNormalizedCoords);
}

TEST_F(KernelDataTest, GivenDataParameterKernelArgumentWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 0;
    uint32_t dataOffset = 20;
    uint32_t dataSize = sizeof(uint32_t);

    SPatchDataParameterBuffer dataParameterTokens[2];

    dataParameterTokens[0].Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterTokens[0].Size = sizeof(SPatchDataParameterBuffer);
    dataParameterTokens[0].Type = DATA_PARAMETER_KERNEL_ARGUMENT;
    dataParameterTokens[0].ArgumentNumber = argumentNumber;
    dataParameterTokens[0].Offset = dataOffset + dataSize * 0;
    dataParameterTokens[0].DataSize = dataSize;
    dataParameterTokens[0].SourceOffset = 0;
    dataParameterTokens[0].LocationIndex = 0x0;
    dataParameterTokens[0].LocationIndex2 = 0x0;

    dataParameterTokens[1].Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterTokens[1].Size = sizeof(SPatchDataParameterBuffer);
    dataParameterTokens[1].Type = DATA_PARAMETER_KERNEL_ARGUMENT;
    dataParameterTokens[1].ArgumentNumber = argumentNumber;
    dataParameterTokens[1].Offset = dataOffset + dataSize * 1;
    dataParameterTokens[1].DataSize = dataSize;
    dataParameterTokens[1].SourceOffset = dataSize * 1;
    dataParameterTokens[1].LocationIndex = 0x0;
    dataParameterTokens[1].LocationIndex2 = 0x0;

    pPatchList = &dataParameterTokens[0];
    patchListSize = dataParameterTokens[0].Size * (sizeof(dataParameterTokens) / sizeof(SPatchDataParameterBuffer));

    buildAndDecode();

    ASSERT_EQ(1u, pKernelInfo->getExplicitArgs().size());

    auto &elements = pKernelInfo->getArgDescriptorAt(0).as<ArgDescValue>().elements;
    ASSERT_EQ(2u, elements.size());

    EXPECT_EQ(dataSize, elements[0].size);
    EXPECT_EQ(dataOffset + dataSize * 0, elements[0].offset);
    EXPECT_EQ(dataSize, elements[1].size);
    EXPECT_EQ(dataOffset + dataSize * 1, elements[1].offset);
}

TEST_F(KernelDataTest, GivenPatchTokenAllocateLocalSurfaceWhenBuildingThenProgramIsCorrect) {

    SPatchAllocateLocalSurface slmToken;
    slmToken.TotalInlineLocalMemorySize = 1024;
    slmToken.Size = sizeof(SPatchAllocateLocalSurface);
    slmToken.Token = PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE;

    pPatchList = &slmToken;
    patchListSize = slmToken.Size;

    buildAndDecode();

    EXPECT_EQ(1024u, pKernelInfo->kernelDescriptor.kernelAttributes.slmInlineSize);
}

TEST_F(KernelDataTest, GivenPatchTokenAllocateStatelessPrintfSurfaceWhenBuildingThenProgramIsCorrect) {
    SPatchAllocateStatelessPrintfSurface printfSurface;
    printfSurface.Token = PATCH_TOKEN_ALLOCATE_STATELESS_PRINTF_SURFACE;
    printfSurface.Size = static_cast<uint32_t>(sizeof(SPatchAllocateStatelessPrintfSurface));

    printfSurface.PrintfSurfaceIndex = 33;
    printfSurface.SurfaceStateHeapOffset = 0x1FF0;
    printfSurface.DataParamOffset = 0x3FF0;
    printfSurface.DataParamSize = 0xFF;

    pPatchList = &printfSurface;
    patchListSize = printfSurface.Size;

    buildAndDecode();

    EXPECT_TRUE(pKernelInfo->kernelDescriptor.kernelAttributes.flags.usesPrintf);
    EXPECT_EQ(printfSurface.SurfaceStateHeapOffset, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.bindful);
    EXPECT_EQ(printfSurface.DataParamOffset, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.stateless);
    EXPECT_EQ(printfSurface.DataParamSize, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.pointerSize);
}

TEST_F(KernelDataTest, GivenPatchTokenSamplerStateArrayWhenBuildingThenProgramIsCorrect) {
    SPatchSamplerStateArray token;
    token.Token = PATCH_TOKEN_SAMPLER_STATE_ARRAY;
    token.Size = static_cast<uint32_t>(sizeof(SPatchSamplerStateArray));

    token.Offset = 33;
    token.Count = 0xF0;
    token.BorderColorOffset = 0x3FF0;

    pPatchList = &token;
    patchListSize = token.Size;

    buildAndDecode();

    EXPECT_EQ_VAL(token.Offset, pKernelInfo->kernelDescriptor.payloadMappings.samplerTable.tableOffset);
    EXPECT_EQ_VAL(token.Count, pKernelInfo->kernelDescriptor.payloadMappings.samplerTable.numSamplers);
    EXPECT_EQ_VAL(token.BorderColorOffset, pKernelInfo->kernelDescriptor.payloadMappings.samplerTable.borderColor);
}

TEST_F(KernelDataTest, GivenPatchTokenAllocateStatelessPrivateMemoryWhenBuildingThenProgramIsCorrect) {
    SPatchAllocateStatelessPrivateSurface token;
    token.Token = PATCH_TOKEN_ALLOCATE_STATELESS_PRIVATE_MEMORY;
    token.Size = static_cast<uint32_t>(sizeof(SPatchAllocateStatelessPrivateSurface));

    token.SurfaceStateHeapOffset = 64;
    token.DataParamOffset = 40;
    token.DataParamSize = 8;
    token.PerThreadPrivateMemorySize = 112;

    pPatchList = &token;
    patchListSize = token.Size;

    buildAndDecode();

    EXPECT_EQ_VAL(token.SurfaceStateHeapOffset, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.bindful);
    EXPECT_EQ_VAL(token.DataParamOffset, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.stateless);
    EXPECT_EQ_VAL(token.DataParamSize, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.pointerSize);
    EXPECT_EQ_VAL(PatchTokenBinary::getPerHwThreadPrivateSurfaceSize(token, pKernelInfo->kernelDescriptor.kernelAttributes.simdSize), pKernelInfo->kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize);
}

TEST_F(KernelDataTest, GivenDataParameterVmeMbBlockTypeWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 16;
    uint32_t offsetVmeMbBlockType = 0xaa;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_VME_MB_BLOCK_TYPE;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetVmeMbBlockType;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());

    EXPECT_TRUE(pKernelInfo->getArgDescriptorAt(argumentNumber).getExtendedTypeInfo().hasVmeExtendedDescriptor);
    ASSERT_EQ(2U, pKernelInfo->kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors.size());
    auto vmeArgDesc = reinterpret_cast<NEO::ArgDescVme *>(pKernelInfo->kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors[1].get());
    EXPECT_EQ(offsetVmeMbBlockType, vmeArgDesc->mbBlockType);
}

TEST_F(KernelDataTest, GivenDataParameterDataVmeSubpixelModeWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 17;
    uint32_t offsetVmeSubpixelMode = 0xab;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_VME_SUBPIXEL_MODE;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetVmeSubpixelMode;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());

    EXPECT_TRUE(pKernelInfo->getArgDescriptorAt(argumentNumber).getExtendedTypeInfo().hasVmeExtendedDescriptor);
    ASSERT_EQ(2U, pKernelInfo->kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors.size());
    auto vmeArgDesc = reinterpret_cast<NEO::ArgDescVme *>(pKernelInfo->kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors[1].get());
    EXPECT_EQ(offsetVmeSubpixelMode, vmeArgDesc->subpixelMode);
}

TEST_F(KernelDataTest, GivenDataParameterVmeSadAdjustModeWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 18;
    uint32_t offsetVmeSadAdjustMode = 0xac;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_VME_SAD_ADJUST_MODE;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetVmeSadAdjustMode;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());

    EXPECT_TRUE(pKernelInfo->getArgDescriptorAt(argumentNumber).getExtendedTypeInfo().hasVmeExtendedDescriptor);
    ASSERT_EQ(2U, pKernelInfo->kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors.size());
    auto vmeArgDesc = reinterpret_cast<NEO::ArgDescVme *>(pKernelInfo->kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors[1].get());
    EXPECT_EQ(offsetVmeSadAdjustMode, vmeArgDesc->sadAdjustMode);
}

TEST_F(KernelDataTest, GivenDataParameterVmeSearchPathTypeWhenBuildingThenProgramIsCorrect) {
    uint32_t argumentNumber = 1;
    uint32_t alignment = 19;
    uint32_t offsetVmeSearchPathType = 0xad;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_VME_SEARCH_PATH_TYPE;
    dataParameterToken.ArgumentNumber = argumentNumber;
    dataParameterToken.Offset = offsetVmeSearchPathType;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = alignment;
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(2U, pKernelInfo->getExplicitArgs().size());

    EXPECT_TRUE(pKernelInfo->getArgDescriptorAt(argumentNumber).getExtendedTypeInfo().hasVmeExtendedDescriptor);
    ASSERT_EQ(2U, pKernelInfo->kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors.size());
    auto vmeArgDesc = reinterpret_cast<NEO::ArgDescVme *>(pKernelInfo->kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors[1].get());
    EXPECT_EQ(offsetVmeSearchPathType, vmeArgDesc->searchPathType);
}

TEST_F(KernelDataTest, GivenPatchTokenStateSipWhenBuildingThenProgramIsCorrect) {
    SPatchStateSIP token;
    token.Token = PATCH_TOKEN_STATE_SIP;
    token.Size = static_cast<uint32_t>(sizeof(SPatchStateSIP));

    token.SystemKernelOffset = 33;

    pPatchList = &token;
    patchListSize = token.Size;

    buildAndDecode();

    EXPECT_EQ(0U, pKernelInfo->getExplicitArgs().size());
    EXPECT_EQ_VAL(token.SystemKernelOffset, pKernelInfo->systemKernelOffset);
}

TEST_F(KernelDataTest, givenSymbolTablePatchTokenThenLinkerInputIsCreated) {
    SPatchFunctionTableInfo token;
    token.Token = PATCH_TOKEN_PROGRAM_SYMBOL_TABLE;
    token.Size = static_cast<uint32_t>(sizeof(SPatchFunctionTableInfo));
    token.NumEntries = 0;

    pPatchList = &token;
    patchListSize = token.Size;

    buildAndDecode();

    EXPECT_NE(nullptr, program->getLinkerInput(pContext->getDevice(0)->getRootDeviceIndex()));
}

TEST_F(KernelDataTest, givenRelocationTablePatchTokenThenLinkerInputIsCreated) {
    SPatchFunctionTableInfo token;
    token.Token = PATCH_TOKEN_PROGRAM_RELOCATION_TABLE;
    token.Size = static_cast<uint32_t>(sizeof(SPatchFunctionTableInfo));
    token.NumEntries = 0;
    kernelHeapSize = 0x100; // force creating kernel allocation for ISA
    auto kernelHeapData = std::make_unique<char[]>(kernelHeapSize);
    pKernelHeap = kernelHeapData.get();

    pPatchList = &token;
    patchListSize = token.Size;

    buildAndDecode();

    EXPECT_NE(nullptr, program->getLinkerInput(pContext->getDevice(0)->getRootDeviceIndex()));
}
