/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/string.h"
#include "runtime/platform/platform.h"
#include "runtime/program/program.h"
#include "unit_tests/fixtures/kernel_data_fixture.h"
#include "unit_tests/helpers/gtest_helpers.h"

TEST_F(KernelDataTest, KernelInfo_Name) {
    kernelName = "myTestKernel";
    kernelNameSize = (uint32_t)alignUp(strlen(kernelName.c_str()) + 1, sizeof(uint32_t));

    buildAndDecode();
}

TEST_F(KernelDataTest, KernelInfo_Heaps) {
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

TEST_F(KernelDataTest, MediaIDLoad) {
    iOpenCL::SPatchMediaInterfaceDescriptorLoad mediaIdLoad;
    mediaIdLoad.Token = PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    mediaIdLoad.Size = sizeof(SPatchMediaInterfaceDescriptorLoad);
    mediaIdLoad.InterfaceDescriptorDataOffset = 0xabcd;

    pPatchList = &mediaIdLoad;
    patchListSize = mediaIdLoad.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD, pKernelInfo->patchInfo.interfaceDescriptorDataLoad->Token);
}

TEST_F(KernelDataTest, AllocateLocalSurface) {
    iOpenCL::SPatchAllocateLocalSurface allocateLocalSurface;
    allocateLocalSurface.Token = PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE;
    allocateLocalSurface.Size = sizeof(SPatchAllocateLocalSurface);
    allocateLocalSurface.Offset = 0;                     // think this is SSH offset for local memory when we used to have surface state for local memory
    allocateLocalSurface.TotalInlineLocalMemorySize = 4; // 4 bytes of local memory just for test

    pPatchList = &allocateLocalSurface;
    patchListSize = allocateLocalSurface.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE, pKernelInfo->patchInfo.localsurface->Token);
    EXPECT_EQ_VAL(allocateLocalSurface.TotalInlineLocalMemorySize, pKernelInfo->patchInfo.localsurface->TotalInlineLocalMemorySize);
}

TEST_F(KernelDataTest, AllocateStatelessConstantMemoryWithInit) {
    iOpenCL::SPatchAllocateStatelessConstantMemorySurfaceWithInitialization allocateStatelessConstantMemoryWithInit;
    allocateStatelessConstantMemoryWithInit.Token = PATCH_TOKEN_ALLOCATE_STATELESS_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION;
    allocateStatelessConstantMemoryWithInit.Size = sizeof(SPatchAllocateStatelessConstantMemorySurfaceWithInitialization);

    allocateStatelessConstantMemoryWithInit.ConstantBufferIndex = 0;
    allocateStatelessConstantMemoryWithInit.SurfaceStateHeapOffset = 0xddu;

    pPatchList = &allocateStatelessConstantMemoryWithInit;
    patchListSize = allocateStatelessConstantMemoryWithInit.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_ALLOCATE_STATELESS_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION, pKernelInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization->Token);
    EXPECT_EQ_VAL(0xddu, pKernelInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization->SurfaceStateHeapOffset);
}

TEST_F(KernelDataTest, AllocateStatelessGlobalMemoryWithInit) {
    iOpenCL::SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization allocateStatelessGlobalMemoryWithInit;
    allocateStatelessGlobalMemoryWithInit.Token = PATCH_TOKEN_ALLOCATE_STATELESS_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION;
    allocateStatelessGlobalMemoryWithInit.Size = sizeof(SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization);

    allocateStatelessGlobalMemoryWithInit.GlobalBufferIndex = 0;
    allocateStatelessGlobalMemoryWithInit.SurfaceStateHeapOffset = 0xddu;

    pPatchList = &allocateStatelessGlobalMemoryWithInit;
    patchListSize = allocateStatelessGlobalMemoryWithInit.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_ALLOCATE_STATELESS_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION, pKernelInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization->Token);
    EXPECT_EQ_VAL(0xddu, pKernelInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization->SurfaceStateHeapOffset);
}

TEST_F(KernelDataTest, PrintfString) {
    char stringValue[] = "%d\n";
    size_t strSize = strlen(stringValue) + 1;

    iOpenCL::SPatchString printfString;
    printfString.Token = PATCH_TOKEN_STRING;
    printfString.Size = static_cast<uint32_t>(sizeof(SPatchString) + strSize);

    printfString.Index = 0;
    printfString.StringSize = static_cast<uint32_t>(strSize);

    cl_char *pPrintfString = new cl_char[printfString.Size];

    memcpy_s(pPrintfString, sizeof(SPatchString), &printfString, sizeof(SPatchString));

    memcpy_s((cl_char *)pPrintfString + sizeof(printfString), strSize, stringValue, strSize);

    pPatchList = (void *)pPrintfString;
    patchListSize = printfString.Size;

    buildAndDecode();

    EXPECT_EQ_VAL(0, strcmp(stringValue, pKernelInfo->queryPrintfString(0)));
    delete[] pPrintfString;
}

TEST_F(KernelDataTest, MediaVFEState) {
    iOpenCL::SPatchMediaVFEState MediaVFEState;
    MediaVFEState.Token = PATCH_TOKEN_MEDIA_VFE_STATE;
    MediaVFEState.Size = sizeof(SPatchMediaVFEState);
    MediaVFEState.PerThreadScratchSpace = 1; // lets say 1KB of perThreadScratchSpace
    MediaVFEState.ScratchSpaceOffset = 0;

    pPatchList = &MediaVFEState;
    patchListSize = MediaVFEState.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_MEDIA_VFE_STATE, pKernelInfo->patchInfo.mediavfestate->Token);
    EXPECT_EQ_VAL(MediaVFEState.PerThreadScratchSpace, pKernelInfo->patchInfo.mediavfestate->PerThreadScratchSpace);
    EXPECT_EQ_VAL(MediaVFEState.ScratchSpaceOffset, pKernelInfo->patchInfo.mediavfestate->ScratchSpaceOffset);
}

TEST_F(KernelDataTest, MediaIDData) {
    iOpenCL::SPatchInterfaceDescriptorData idData;
    idData.Token = PATCH_TOKEN_INTERFACE_DESCRIPTOR_DATA;
    idData.Size = sizeof(SPatchInterfaceDescriptorData);
    idData.BindingTableOffset = 0xaa;
    idData.KernelOffset = 0xbb;
    idData.Offset = 0xcc;
    idData.SamplerStateOffset = 0xdd;

    pPatchList = &idData;
    patchListSize = idData.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_INTERFACE_DESCRIPTOR_DATA, pKernelInfo->patchInfo.interfaceDescriptorData->Token);
}

TEST_F(KernelDataTest, SamplerArgument) {
    iOpenCL::SPatchSamplerKernelArgument samplerData;
    samplerData.Token = PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT;
    samplerData.ArgumentNumber = 3;
    samplerData.Offset = 0x40;
    samplerData.Type = iOpenCL::SAMPLER_OBJECT_TEXTURE;
    samplerData.Size = sizeof(samplerData);

    pPatchList = &samplerData;
    patchListSize = samplerData.Size;

    buildAndDecode();

    EXPECT_TRUE(pKernelInfo->kernelArgInfo[3].isSampler);
    EXPECT_EQ_VAL(samplerData.Offset, pKernelInfo->kernelArgInfo[3].offsetHeap);
}

TEST_F(KernelDataTest, AcceleratorArgument) {
    iOpenCL::SPatchSamplerKernelArgument samplerData;
    samplerData.Token = PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT;
    samplerData.ArgumentNumber = 3;
    samplerData.Offset = 0x40;
    samplerData.Type = iOpenCL::SAMPLER_OBJECT_VME;
    samplerData.Size = sizeof(samplerData);

    pPatchList = &samplerData;
    patchListSize = samplerData.Size;

    buildAndDecode();

    EXPECT_TRUE(pKernelInfo->kernelArgInfo[3].isAccelerator);
    EXPECT_EQ_VAL(samplerData.Offset, pKernelInfo->kernelArgInfo[3].offsetHeap);
}

TEST_F(KernelDataTest, BindingTableState) {
    iOpenCL::SPatchBindingTableState bindingTableState;
    bindingTableState.Token = PATCH_TOKEN_BINDING_TABLE_STATE;
    bindingTableState.Size = sizeof(SPatchBindingTableState);
    bindingTableState.Count = 0xaa;
    bindingTableState.Offset = 0xbb;
    bindingTableState.SurfaceStateOffset = 0xcc;

    pPatchList = &bindingTableState;
    patchListSize = bindingTableState.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_BINDING_TABLE_STATE, pKernelInfo->patchInfo.bindingTableState->Token);
}

TEST_F(KernelDataTest, DataParameterStream) {
    iOpenCL::SPatchDataParameterStream dataParameterStream;
    dataParameterStream.Token = PATCH_TOKEN_DATA_PARAMETER_STREAM;
    dataParameterStream.Size = sizeof(SPatchDataParameterStream);
    dataParameterStream.DataParameterStreamSize = 0x10;

    pPatchList = &dataParameterStream;
    patchListSize = dataParameterStream.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_STREAM, pKernelInfo->patchInfo.dataParameterStream->Token);
}

TEST_F(KernelDataTest, ThreadPayload) {
    iOpenCL::SPatchThreadPayload threadPayload;
    threadPayload.Token = PATCH_TOKEN_THREAD_PAYLOAD;
    threadPayload.Size = sizeof(SPatchThreadPayload);
    threadPayload.GetGlobalOffsetPresent = true;
    threadPayload.GetGroupIDPresent = true;
    threadPayload.GetLocalIDPresent = true;
    threadPayload.HeaderPresent = true;
    threadPayload.IndirectPayloadStorage = true;
    threadPayload.LocalIDFlattenedPresent = true;
    threadPayload.LocalIDXPresent = true;
    threadPayload.LocalIDYPresent = true;
    threadPayload.LocalIDZPresent = true;

    pPatchList = &threadPayload;
    patchListSize = threadPayload.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_THREAD_PAYLOAD, pKernelInfo->patchInfo.threadPayload->Token);
}

TEST_F(KernelDataTest, ExecutionEnvironmentNoReqdWorkGroupSize) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment;
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);
    executionEnvironment.RequiredWorkGroupSizeX = 0;
    executionEnvironment.RequiredWorkGroupSizeY = 0;
    executionEnvironment.RequiredWorkGroupSizeZ = 0;
    executionEnvironment.LargestCompiledSIMDSize = 32;
    executionEnvironment.CompiledSubGroupsNumber = 0xaa;
    executionEnvironment.HasBarriers = false;
    executionEnvironment.DisableMidThreadPreemption = true;
    executionEnvironment.CompiledSIMD16 = false;
    executionEnvironment.CompiledSIMD32 = true;
    executionEnvironment.CompiledSIMD8 = false;
    executionEnvironment.HasDeviceEnqueue = false;
    executionEnvironment.MayAccessUndeclaredResource = false;
    executionEnvironment.UsesFencesForReadWriteImages = false;
    executionEnvironment.UsesStatelessSpillFill = false;
    executionEnvironment.IsCoherent = true;
    executionEnvironment.IsInitializer = false;
    executionEnvironment.IsFinalizer = false;
    executionEnvironment.SubgroupIndependentForwardProgressRequired = false;
    executionEnvironment.CompiledForGreaterThan4GBBuffers = false;

    pPatchList = &executionEnvironment;
    patchListSize = executionEnvironment.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_EXECUTION_ENVIRONMENT, pKernelInfo->patchInfo.executionEnvironment->Token);
    EXPECT_EQ_VAL(WorkloadInfo::undefinedOffset, pKernelInfo->reqdWorkGroupSize[0]);
    EXPECT_EQ_VAL(WorkloadInfo::undefinedOffset, pKernelInfo->reqdWorkGroupSize[1]);
    EXPECT_EQ_VAL(WorkloadInfo::undefinedOffset, pKernelInfo->reqdWorkGroupSize[2]);
}

TEST_F(KernelDataTest, ExecutionEnvironment) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment;
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);
    executionEnvironment.RequiredWorkGroupSizeX = 32;
    executionEnvironment.RequiredWorkGroupSizeY = 16;
    executionEnvironment.RequiredWorkGroupSizeZ = 8;
    executionEnvironment.LargestCompiledSIMDSize = 32;
    executionEnvironment.CompiledSubGroupsNumber = 0xaa;
    executionEnvironment.HasBarriers = false;
    executionEnvironment.DisableMidThreadPreemption = true;
    executionEnvironment.CompiledSIMD16 = false;
    executionEnvironment.CompiledSIMD32 = true;
    executionEnvironment.CompiledSIMD8 = false;
    executionEnvironment.HasDeviceEnqueue = false;
    executionEnvironment.MayAccessUndeclaredResource = false;
    executionEnvironment.UsesFencesForReadWriteImages = false;
    executionEnvironment.UsesStatelessSpillFill = false;
    executionEnvironment.IsCoherent = true;
    executionEnvironment.IsInitializer = false;
    executionEnvironment.IsFinalizer = false;
    executionEnvironment.SubgroupIndependentForwardProgressRequired = false;
    executionEnvironment.CompiledForGreaterThan4GBBuffers = false;

    pPatchList = &executionEnvironment;
    patchListSize = executionEnvironment.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_EXECUTION_ENVIRONMENT, pKernelInfo->patchInfo.executionEnvironment->Token);
    EXPECT_EQ(32u, pKernelInfo->reqdWorkGroupSize[0]);
    EXPECT_EQ(16u, pKernelInfo->reqdWorkGroupSize[1]);
    EXPECT_EQ(8u, pKernelInfo->reqdWorkGroupSize[2]);
    EXPECT_TRUE(pKernelInfo->requiresSshForBuffers);
}

TEST_F(KernelDataTest, ExecutionEnvironmentCompiledForGreaterThan4GBBuffers) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment;
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);
    executionEnvironment.RequiredWorkGroupSizeX = 32;
    executionEnvironment.RequiredWorkGroupSizeY = 16;
    executionEnvironment.RequiredWorkGroupSizeZ = 8;
    executionEnvironment.LargestCompiledSIMDSize = 32;
    executionEnvironment.CompiledSubGroupsNumber = 0xaa;
    executionEnvironment.HasBarriers = false;
    executionEnvironment.DisableMidThreadPreemption = true;
    executionEnvironment.CompiledSIMD16 = false;
    executionEnvironment.CompiledSIMD32 = true;
    executionEnvironment.CompiledSIMD8 = false;
    executionEnvironment.HasDeviceEnqueue = false;
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

    EXPECT_EQ_CONST(PATCH_TOKEN_EXECUTION_ENVIRONMENT, pKernelInfo->patchInfo.executionEnvironment->Token);
    EXPECT_FALSE(pKernelInfo->requiresSshForBuffers);
}

TEST_F(KernelDataTest, ExecutionEnvironmentDoesntHaveDeviceEnqueue) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);
    executionEnvironment.HasDeviceEnqueue = false;

    pPatchList = &executionEnvironment;
    patchListSize = executionEnvironment.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_EXECUTION_ENVIRONMENT, pKernelInfo->patchInfo.executionEnvironment->Token);
    EXPECT_EQ_VAL(0u, program->getParentKernelInfoArray().size());
}

TEST_F(KernelDataTest, ExecutionEnvironmentHasDeviceEnqueue) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);
    executionEnvironment.HasDeviceEnqueue = true;

    pPatchList = &executionEnvironment;
    patchListSize = executionEnvironment.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_EXECUTION_ENVIRONMENT, pKernelInfo->patchInfo.executionEnvironment->Token);
    EXPECT_EQ_VAL(1u, program->getParentKernelInfoArray().size());
}

TEST_F(KernelDataTest, ExecutionEnvironmentDoesntRequireSubgroupIndependentForwardProgress) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);
    executionEnvironment.SubgroupIndependentForwardProgressRequired = false;

    pPatchList = &executionEnvironment;
    patchListSize = executionEnvironment.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_EXECUTION_ENVIRONMENT, pKernelInfo->patchInfo.executionEnvironment->Token);
    EXPECT_EQ_VAL(0u, program->getSubgroupKernelInfoArray().size());
}

TEST_F(KernelDataTest, ExecutionEnvironmentRequiresSubgroupIndependentForwardProgress) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);
    executionEnvironment.SubgroupIndependentForwardProgressRequired = true;

    pPatchList = &executionEnvironment;
    patchListSize = executionEnvironment.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_EXECUTION_ENVIRONMENT, pKernelInfo->patchInfo.executionEnvironment->Token);
    EXPECT_EQ_VAL(1u, program->getSubgroupKernelInfoArray().size());
}

TEST_F(KernelDataTest, KernelAttributesInfo) {
    iOpenCL::SPatchKernelAttributesInfo kernelAttributesInfo;
    kernelAttributesInfo.Token = PATCH_TOKEN_KERNEL_ATTRIBUTES_INFO;
    kernelAttributesInfo.Size = sizeof(SPatchKernelAttributesInfo);
    kernelAttributesInfo.AttributesSize = 0x10;

    pPatchList = &kernelAttributesInfo;
    patchListSize = kernelAttributesInfo.Size;

    buildAndDecode();

    EXPECT_EQ_CONST(PATCH_TOKEN_KERNEL_ATTRIBUTES_INFO, pKernelInfo->patchInfo.pKernelAttributesInfo->Token);
}

TEST_F(KernelDataTest, WhenDecodingExecutionEnvironmentTokenThenWalkOrderIsForcedToXMajor) {
    iOpenCL::SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.Token = PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    executionEnvironment.Size = sizeof(SPatchExecutionEnvironment);

    pPatchList = &executionEnvironment;
    patchListSize = executionEnvironment.Size;

    buildAndDecode();

    std::array<uint8_t, 3> expectedWalkOrder = {{0, 1, 2}};
    std::array<uint8_t, 3> expectedDimsIds = {{0, 1, 2}};
    EXPECT_EQ(expectedWalkOrder, pKernelInfo->workgroupWalkOrder);
    EXPECT_EQ(expectedDimsIds, pKernelInfo->workgroupDimensionsOrder);
}

// Test all the different data parameters with the same "made up" data
class DataParameterTest : public KernelDataTest, public testing::WithParamInterface<uint32_t> {};

TEST_P(DataParameterTest, DataParameterTests) {
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

    if (pKernelInfo->patchInfo.dataParameterBuffers.size() > 0) {
        EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
        EXPECT_EQ_VAL(GetParam(), pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);
        if (pKernelInfo->kernelArgInfo.size() == dataParameterToken.ArgumentNumber + 1) {
            if (GetParam() == DATA_PARAMETER_BUFFER_STATEFUL) {
                EXPECT_TRUE(pKernelInfo->kernelArgInfo[dataParameterToken.ArgumentNumber].pureStatefulBufferAccess);
            } else {
                EXPECT_FALSE(pKernelInfo->kernelArgInfo[dataParameterToken.ArgumentNumber].pureStatefulBufferAccess);
            }
        } // no else - some params are skipped
    }
}

// note that we start at '2' because we test kernel arg tokens elsewhere
INSTANTIATE_TEST_CASE_P(DataParameterTests,
                        DataParameterTest,
                        testing::Range(2u, static_cast<uint32_t>(NUM_DATA_PARAMETER_TOKENS)));

class KernelDataParameterTest : public KernelDataTest {};
TEST_F(KernelDataParameterTest, DataParameterTestsDataPatameterBufferOffset) {
    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_BUFFER_OFFSET;
    dataParameterToken.ArgumentNumber = 1;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;
    dataParameterToken.Offset = 0;
    dataParameterToken.SourceOffset = 8;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    ASSERT_EQ(1u, pKernelInfo->patchInfo.dataParameterBuffers.size());
    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_BUFFER_OFFSET, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);
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

    ASSERT_EQ(1u, pKernelInfo->patchInfo.dataParameterBuffers.size());
    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_BUFFER_STATEFUL, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);
    EXPECT_TRUE(pKernelInfo->kernelArgInfo[dataParameterToken.ArgumentNumber].pureStatefulBufferAccess);
}

TEST_F(KernelDataParameterTest, givenUnknownDataParameterWhenDecodedThenParameterIsIgnored) {
    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = NUM_DATA_PARAMETER_TOKENS + 1;
    dataParameterToken.ArgumentNumber = 1;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.LocationIndex = 0x0;
    dataParameterToken.LocationIndex2 = 0x0;
    dataParameterToken.Offset = 0;
    dataParameterToken.SourceOffset = 8;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    EXPECT_EQ_VAL(0u, pKernelInfo->patchInfo.dataParameterBuffers.size());
}

TEST_F(KernelDataTest, DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(alignment, pKernelInfo->kernelArgInfo[argumentNumber].slmAlignment);
    EXPECT_EQ(offsetCrossThread, pKernelInfo->kernelArgInfo[argumentNumber].kernelArgPatchInfoVector[0].crossthreadOffset);
}

TEST_F(KernelDataTest, DATA_PARAMETER_IMAGE_WIDTH) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_IMAGE_WIDTH, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetImgWidth, pKernelInfo->kernelArgInfo[argumentNumber].offsetImgWidth);
}

TEST_F(KernelDataTest, DATA_PARAMETER_IMAGE_HEIGHT) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_IMAGE_HEIGHT, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetImgHeight, pKernelInfo->kernelArgInfo[argumentNumber].offsetImgHeight);
}

TEST_F(KernelDataTest, DATA_PARAMETER_IMAGE_DEPTH) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_IMAGE_DEPTH, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetImgDepth, pKernelInfo->kernelArgInfo[argumentNumber].offsetImgDepth);
}

TEST_F(KernelDataTest, DATA_PARAMETER_IMAGE_NUM_SAMPLES) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_IMAGE_NUM_SAMPLES, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetNumSamples, pKernelInfo->kernelArgInfo[argumentNumber].offsetNumSamples);
}

TEST_F(KernelDataTest, DATA_PARAMETER_IMAGE_NUM_MIP_LEVELS) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_IMAGE_NUM_MIP_LEVELS, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetNumMipLevels, pKernelInfo->kernelArgInfo[argumentNumber].offsetNumMipLevels);
}

TEST_F(KernelDataTest, DATA_PARAMETER_IMAGE_DATA_TYPE) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetChannelDataType, pKernelInfo->kernelArgInfo[argumentNumber].offsetChannelDataType);
}

TEST_F(KernelDataTest, DATA_PARAMETER_IMAGE_CHANNEL_ORDER) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_IMAGE_CHANNEL_ORDER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetChannelOrder, pKernelInfo->kernelArgInfo[argumentNumber].offsetChannelOrder);
}

TEST_F(KernelDataTest, DATA_PARAMETER_IMAGE_ARRAY_SIZE) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_IMAGE_ARRAY_SIZE, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetImageArraySize, pKernelInfo->kernelArgInfo[argumentNumber].offsetArraySize);
}

TEST_F(KernelDataTest, DATA_PARAMETER_WORK_DIMENSIONS) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_WORK_DIMENSIONS, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetWorkDim, pKernelInfo->workloadInfo.workDimOffset);
}

TEST_F(KernelDataTest, DATA_PARAMETER_SIMD_SIZE) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_SIMD_SIZE, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(0u, pKernelInfo->kernelArgInfo.size());
    EXPECT_EQ(offsetSimdSize, pKernelInfo->workloadInfo.simdSizeOffset);
}

TEST_F(KernelDataTest, DATA_PARAMETER_PRIVATE_MEMORY_STATELESS_SIZE) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_PRIVATE_MEMORY_STATELESS_SIZE, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);
    EXPECT_EQ(0u, pKernelInfo->kernelArgInfo.size());
}

TEST_F(KernelDataTest, DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_SIZE) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_SIZE, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);
    EXPECT_EQ(0u, pKernelInfo->kernelArgInfo.size());
}

TEST_F(KernelDataTest, DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_START_ADDRESS) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_START_ADDRESS, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);
    EXPECT_EQ(0u, pKernelInfo->kernelArgInfo.size());
}

TEST_F(KernelDataTest, DATA_PARAMETER_NUM_WORK_GROUPS) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_NUM_WORK_GROUPS, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetNumWorkGroups[argumentNumber], pKernelInfo->workloadInfo.numWorkGroupsOffset[argumentNumber]);
}

TEST_F(KernelDataTest, DATA_PARAMETER_MAX_WORKGROUP_SIZE) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_MAX_WORKGROUP_SIZE, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetMaxWorkGroupSize, pKernelInfo->workloadInfo.maxWorkGroupSizeOffset);
}

TEST_F(KernelDataTest, DATA_PARAMETER_SAMPLER_ADDRESS_MODE) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_SAMPLER_ADDRESS_MODE, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    ASSERT_EQ(1u, pKernelInfo->kernelArgInfo.size());

    EXPECT_EQ(dataOffset, pKernelInfo->kernelArgInfo[0].offsetSamplerAddressingMode);
}

TEST_F(KernelDataTest, DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    ASSERT_EQ(2u, pKernelInfo->kernelArgInfo.size());

    EXPECT_EQ(dataOffset, pKernelInfo->kernelArgInfo[1].offsetSamplerSnapWa);
}

TEST_F(KernelDataTest, DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    ASSERT_EQ(2u, pKernelInfo->kernelArgInfo.size());

    EXPECT_EQ(dataOffset, pKernelInfo->kernelArgInfo[1].offsetSamplerNormalizedCoords);
}

TEST_F(KernelDataTest, DATA_PARAMETER_KERNEL_ARGUMENT) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_KERNEL_ARGUMENT, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    ASSERT_EQ(1u, pKernelInfo->kernelArgInfo.size());
    ASSERT_EQ(2u, pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.size());

    ASSERT_EQ(dataSize, pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size);
    EXPECT_EQ(dataOffset + dataSize * 0, pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    ASSERT_EQ(dataSize, pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[1].size);
    EXPECT_EQ(dataOffset + dataSize * 1, pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[1].crossthreadOffset);
}

TEST_F(KernelDataTest, PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE) {

    SPatchAllocateLocalSurface slmToken;
    slmToken.TotalInlineLocalMemorySize = 1024;
    slmToken.Size = sizeof(SPatchAllocateLocalSurface);
    slmToken.Token = PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE;

    pPatchList = &slmToken;
    patchListSize = slmToken.Size;

    buildAndDecode();

    EXPECT_EQ(1024u, pKernelInfo->workloadInfo.slmStaticSize);
}

TEST_F(KernelDataTest, PATCH_TOKEN_ALLOCATE_STATELESS_PRINTF_SURFACE) {
    SPatchAllocateStatelessPrintfSurface printfSurface;
    printfSurface.Token = PATCH_TOKEN_ALLOCATE_STATELESS_PRINTF_SURFACE;
    printfSurface.Size = static_cast<uint32_t>(sizeof(SPatchAllocateStatelessPrintfSurface));

    printfSurface.PrintfSurfaceIndex = 33;
    printfSurface.SurfaceStateHeapOffset = 0x1FF0;
    printfSurface.DataParamOffset = 0x3FF0;
    printfSurface.DataParamSize = 0x1000;

    pPatchList = &printfSurface;
    patchListSize = printfSurface.Size;

    buildAndDecode();

    ASSERT_NE(nullptr, pKernelInfo->patchInfo.pAllocateStatelessPrintfSurface);

    EXPECT_EQ(printfSurface.PrintfSurfaceIndex, pKernelInfo->patchInfo.pAllocateStatelessPrintfSurface->PrintfSurfaceIndex);
    EXPECT_EQ(printfSurface.SurfaceStateHeapOffset, pKernelInfo->patchInfo.pAllocateStatelessPrintfSurface->SurfaceStateHeapOffset);
    EXPECT_EQ(printfSurface.DataParamOffset, pKernelInfo->patchInfo.pAllocateStatelessPrintfSurface->DataParamOffset);
    EXPECT_EQ(printfSurface.DataParamSize, pKernelInfo->patchInfo.pAllocateStatelessPrintfSurface->DataParamSize);
}

TEST_F(KernelDataTest, PATCH_TOKEN_SAMPLER_STATE_ARRAY) {
    SPatchSamplerStateArray token;
    token.Token = PATCH_TOKEN_SAMPLER_STATE_ARRAY;
    token.Size = static_cast<uint32_t>(sizeof(SPatchSamplerStateArray));

    token.Offset = 33;
    token.Count = 0x1FF0;
    token.BorderColorOffset = 0x3FF0;

    pPatchList = &token;
    patchListSize = token.Size;

    buildAndDecode();

    ASSERT_NE(nullptr, pKernelInfo->patchInfo.samplerStateArray);

    EXPECT_EQ_VAL(token.Offset, pKernelInfo->patchInfo.samplerStateArray->Offset);
    EXPECT_EQ_VAL(token.Count, pKernelInfo->patchInfo.samplerStateArray->Count);
    EXPECT_EQ_VAL(token.BorderColorOffset, pKernelInfo->patchInfo.samplerStateArray->BorderColorOffset);
}

TEST_F(KernelDataTest, PATCH_TOKEN_ALLOCATE_STATELESS_PRIVATE_MEMORY) {
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

    ASSERT_NE(nullptr, pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface);

    EXPECT_EQ_VAL(token.SurfaceStateHeapOffset, pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface->SurfaceStateHeapOffset);
    EXPECT_EQ_VAL(token.DataParamOffset, pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface->DataParamOffset);
    EXPECT_EQ_VAL(token.DataParamSize, pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface->DataParamSize);
    EXPECT_EQ_VAL(token.PerThreadPrivateMemorySize, pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize);
}

TEST_F(KernelDataTest, DATA_PARAMETER_VME_MB_BLOCK_TYPE) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_VME_MB_BLOCK_TYPE, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetVmeMbBlockType, pKernelInfo->kernelArgInfo[argumentNumber].offsetVmeMbBlockType);
}

TEST_F(KernelDataTest, DATA_PARAMETER_VME_SUBPIXEL_MODE) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_VME_SUBPIXEL_MODE, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetVmeSubpixelMode, pKernelInfo->kernelArgInfo[argumentNumber].offsetVmeSubpixelMode);
}

TEST_F(KernelDataTest, DATA_PARAMETER_VME_SAD_ADJUST_MODE) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_VME_SAD_ADJUST_MODE, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetVmeSadAdjustMode, pKernelInfo->kernelArgInfo[argumentNumber].offsetVmeSadAdjustMode);
}

TEST_F(KernelDataTest, DATA_PARAMETER_VME_SEARCH_PATH_TYPE) {
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

    EXPECT_EQ_CONST(PATCH_TOKEN_DATA_PARAMETER_BUFFER, pKernelInfo->patchInfo.dataParameterBuffers[0]->Token);
    EXPECT_EQ_VAL(DATA_PARAMETER_VME_SEARCH_PATH_TYPE, pKernelInfo->patchInfo.dataParameterBuffers[0]->Type);

    EXPECT_EQ(offsetVmeSearchPathType, pKernelInfo->kernelArgInfo[argumentNumber].offsetVmeSearchPathType);
}

TEST_F(KernelDataTest, PATCH_TOKEN_STATE_SIP) {
    SPatchStateSIP token;
    token.Token = PATCH_TOKEN_STATE_SIP;
    token.Size = static_cast<uint32_t>(sizeof(SPatchStateSIP));

    token.SystemKernelOffset = 33;

    pPatchList = &token;
    patchListSize = token.Size;

    buildAndDecode();

    EXPECT_EQ_VAL(token.SystemKernelOffset, pKernelInfo->systemKernelOffset);
}

TEST_F(KernelDataTest, PATCH_TOKEN_ALLOCATE_SIP_SURFACE) {
    SPatchAllocateSystemThreadSurface token;
    token.Token = PATCH_TOKEN_ALLOCATE_SIP_SURFACE;
    token.Size = static_cast<uint32_t>(sizeof(SPatchAllocateSystemThreadSurface));
    token.Offset = 32;
    token.BTI = 0;
    token.PerThreadSystemThreadSurfaceSize = 0x10000;

    pPatchList = &token;
    patchListSize = token.Size;

    buildAndDecode();

    EXPECT_EQ(0u, pKernelInfo->patchInfo.pAllocateSystemThreadSurface->BTI);
    EXPECT_EQ(token.Offset, pKernelInfo->patchInfo.pAllocateSystemThreadSurface->Offset);
    EXPECT_EQ(token.Token, pKernelInfo->patchInfo.pAllocateSystemThreadSurface->Token);
    EXPECT_EQ(token.PerThreadSystemThreadSurfaceSize, pKernelInfo->patchInfo.pAllocateSystemThreadSurface->PerThreadSystemThreadSurfaceSize);
}
