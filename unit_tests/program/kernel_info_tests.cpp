/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/execution_environment.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/program/kernel_arg_info.h"
#include "runtime/program/kernel_info.h"
#include "unit_tests/fixtures/multi_root_device_fixture.h"
#include "unit_tests/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

#include <memory>
#include <type_traits>

using namespace NEO;

TEST(KernelInfo, KernelInfoHasCopyMoveAssingmentDisabled) {
    static_assert(false == std::is_move_constructible<KernelInfo>::value, "");
    static_assert(false == std::is_copy_constructible<KernelInfo>::value, "");
    static_assert(false == std::is_move_assignable<KernelInfo>::value, "");
    static_assert(false == std::is_copy_assignable<KernelInfo>::value, "");
}

TEST(KernelInfo, whenDefaultConstructedThenUsesSshFlagIsNotSet) {
    KernelInfo kernelInfo;
    EXPECT_FALSE(kernelInfo.usesSsh);
}

TEST(KernelInfo, decodeConstantMemoryKernelArgument) {
    uint32_t argumentNumber = 0;
    auto pKernelInfo = std::make_unique<KernelInfo>();
    SPatchStatelessConstantMemoryObjectKernelArgument arg;
    arg.Token = 0xa;
    arg.Size = 0x20;
    arg.ArgumentNumber = argumentNumber;
    arg.SurfaceStateHeapOffset = 0x30;
    arg.DataParamOffset = 0x40;
    arg.DataParamSize = 0x4;
    arg.LocationIndex = static_cast<uint32_t>(-1);
    arg.LocationIndex2 = static_cast<uint32_t>(-1);

    pKernelInfo->storeKernelArgument(&arg);
    EXPECT_TRUE(pKernelInfo->usesSsh);

    const auto &argInfo = pKernelInfo->kernelArgInfo[argumentNumber];
    EXPECT_EQ(arg.SurfaceStateHeapOffset, argInfo.offsetHeap);
    EXPECT_FALSE(argInfo.isImage);

    const auto &patchInfo = pKernelInfo->patchInfo;
    EXPECT_EQ(1u, patchInfo.statelessGlobalMemObjKernelArgs.size());
}

TEST(KernelInfo, decodeGlobalMemoryKernelArgument) {
    uint32_t argumentNumber = 1;
    auto pKernelInfo = std::make_unique<KernelInfo>();
    SPatchStatelessGlobalMemoryObjectKernelArgument arg;
    arg.Token = 0xb;
    arg.Size = 0x30;
    arg.ArgumentNumber = argumentNumber;
    arg.SurfaceStateHeapOffset = 0x40;
    arg.DataParamOffset = 050;
    arg.DataParamSize = 0x8;
    arg.LocationIndex = static_cast<uint32_t>(-1);
    arg.LocationIndex2 = static_cast<uint32_t>(-1);

    pKernelInfo->storeKernelArgument(&arg);
    EXPECT_TRUE(pKernelInfo->usesSsh);

    const auto &argInfo = pKernelInfo->kernelArgInfo[argumentNumber];
    EXPECT_EQ(arg.SurfaceStateHeapOffset, argInfo.offsetHeap);
    EXPECT_FALSE(argInfo.isImage);

    const auto &patchInfo = pKernelInfo->patchInfo;
    EXPECT_EQ(1u, patchInfo.statelessGlobalMemObjKernelArgs.size());
}

TEST(KernelInfo, decodeImageKernelArgument) {
    uint32_t argumentNumber = 1;
    auto pKernelInfo = std::make_unique<KernelInfo>();
    SPatchImageMemoryObjectKernelArgument arg;
    arg.Token = 0xc;
    arg.Size = 0x20;
    arg.ArgumentNumber = argumentNumber;
    arg.Type = 0x4;
    arg.Offset = 0x40;
    arg.LocationIndex = static_cast<uint32_t>(-1);
    arg.LocationIndex2 = static_cast<uint32_t>(-1);
    arg.Writeable = true;

    pKernelInfo->storeKernelArgument(&arg);
    EXPECT_TRUE(pKernelInfo->usesSsh);

    const auto &argInfo = pKernelInfo->kernelArgInfo[argumentNumber];
    EXPECT_EQ(sizeof(cl_mem), static_cast<size_t>(argInfo.metadata.argByValSize));
    EXPECT_EQ(arg.Offset, argInfo.offsetHeap);
    EXPECT_TRUE(argInfo.isImage);
    EXPECT_EQ(KernelArgMetadata::AccessQualifier::ReadWrite, argInfo.metadata.accessQualifier);
    EXPECT_TRUE(argInfo.metadata.typeQualifiers.empty());
}

TEST(KernelInfoTest, givenKernelInfoWhenCreateKernelAllocationThenCopyWholeKernelHeapToKernelAllocation) {
    KernelInfo kernelInfo;
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    SKernelBinaryHeaderCommon kernelHeader;
    const size_t heapSize = 0x40;
    char heap[heapSize];
    kernelHeader.KernelHeapSize = heapSize;
    kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
    kernelInfo.heapInfo.pKernelHeap = &heap;

    for (size_t i = 0; i < heapSize; i++) {
        heap[i] = static_cast<char>(i);
    }

    auto retVal = kernelInfo.createKernelAllocation(0, &memoryManager);
    EXPECT_TRUE(retVal);
    auto allocation = kernelInfo.kernelAllocation;
    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), heap, heapSize));
    EXPECT_EQ(heapSize, allocation->getUnderlyingBufferSize());
    memoryManager.checkGpuUsageAndDestroyGraphicsAllocations(allocation);
}

class MyMemoryManager : public OsAgnosticMemoryManager {
  public:
    using OsAgnosticMemoryManager::OsAgnosticMemoryManager;
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) override { return nullptr; }
};

TEST(KernelInfoTest, givenKernelInfoWhenCreateKernelAllocationAndCannotAllocateMemoryThenReturnsFalse) {
    KernelInfo kernelInfo;
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MyMemoryManager memoryManager(executionEnvironment);
    SKernelBinaryHeaderCommon kernelHeader;
    kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
    auto retVal = kernelInfo.createKernelAllocation(0, &memoryManager);
    EXPECT_FALSE(retVal);
}

TEST(KernelInfo, decodeGlobalMemObjectKernelArgument) {
    uint32_t argumentNumber = 1;
    auto pKernelInfo = std::make_unique<KernelInfo>();
    SPatchGlobalMemoryObjectKernelArgument arg;
    arg.Token = 0xb;
    arg.Size = 0x10;
    arg.ArgumentNumber = argumentNumber;
    arg.Offset = 0x40;
    arg.LocationIndex = static_cast<uint32_t>(-1);
    arg.LocationIndex2 = static_cast<uint32_t>(-1);

    pKernelInfo->storeKernelArgument(&arg);
    EXPECT_TRUE(pKernelInfo->usesSsh);

    const auto &argInfo = pKernelInfo->kernelArgInfo[argumentNumber];
    EXPECT_EQ(arg.Offset, argInfo.offsetHeap);
    EXPECT_TRUE(argInfo.isBuffer);
}

TEST(KernelInfo, decodeSamplerKernelArgument) {
    uint32_t argumentNumber = 1;
    auto pKernelInfo = std::make_unique<KernelInfo>();
    SPatchSamplerKernelArgument arg;

    arg.ArgumentNumber = argumentNumber;
    arg.Token = 0x10;
    arg.Size = 0x18;
    arg.LocationIndex = static_cast<uint32_t>(-1);
    arg.LocationIndex2 = static_cast<uint32_t>(-1);
    arg.Offset = 0x40;
    arg.Type = iOpenCL::SAMPLER_OBJECT_TEXTURE;

    pKernelInfo->usesSsh = true;

    pKernelInfo->storeKernelArgument(&arg);

    const auto &argInfo = pKernelInfo->kernelArgInfo[argumentNumber];
    EXPECT_EQ(arg.Offset, argInfo.offsetHeap);
    EXPECT_FALSE(argInfo.isImage);
    EXPECT_TRUE(argInfo.isSampler);
    EXPECT_TRUE(pKernelInfo->usesSsh);
}

TEST(KernelInfo, whenStoringArgInfoThenMetadataIsProperlyPopulated) {
    KernelInfo kernelInfo;
    NEO::ArgTypeMetadata metadata;
    metadata.accessQualifier = NEO::KernelArgMetadata::AccessQualifier::WriteOnly;
    metadata.addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::Global;
    metadata.argByValSize = sizeof(void *);
    metadata.typeQualifiers.pipeQual = true;
    auto metadataExtended = std::make_unique<NEO::ArgTypeMetadataExtended>();
    auto metadataExtendedPtr = metadataExtended.get();
    kernelInfo.storeArgInfo(2, metadata, std::move(metadataExtended));

    ASSERT_EQ(3U, kernelInfo.kernelArgInfo.size());
    EXPECT_EQ(metadata.accessQualifier, kernelInfo.kernelArgInfo[2].metadata.accessQualifier);
    EXPECT_EQ(metadata.addressQualifier, kernelInfo.kernelArgInfo[2].metadata.addressQualifier);
    EXPECT_EQ(metadata.argByValSize, kernelInfo.kernelArgInfo[2].metadata.argByValSize);
    EXPECT_EQ(metadata.typeQualifiers.packed, kernelInfo.kernelArgInfo[2].metadata.typeQualifiers.packed);
    EXPECT_EQ(metadataExtendedPtr, kernelInfo.kernelArgInfo[2].metadataExtended.get());
}

TEST(KernelInfo, givenKernelInfoWhenStoreTransformableArgThenArgInfoIsTransformable) {
    uint32_t argumentNumber = 1;
    auto kernelInfo = std::make_unique<KernelInfo>();
    SPatchImageMemoryObjectKernelArgument arg;
    arg.ArgumentNumber = argumentNumber;
    arg.Transformable = true;

    kernelInfo->storeKernelArgument(&arg);
    const auto &argInfo = kernelInfo->kernelArgInfo[argumentNumber];
    EXPECT_TRUE(argInfo.isTransformable);
}

TEST(KernelInfo, givenKernelInfoWhenStoreNonTransformableArgThenArgInfoIsNotTransformable) {
    uint32_t argumentNumber = 1;
    auto kernelInfo = std::make_unique<KernelInfo>();
    SPatchImageMemoryObjectKernelArgument arg;
    arg.ArgumentNumber = argumentNumber;
    arg.Transformable = false;

    kernelInfo->storeKernelArgument(&arg);
    const auto &argInfo = kernelInfo->kernelArgInfo[argumentNumber];
    EXPECT_FALSE(argInfo.isTransformable);
}

using KernelInfoMultiRootDeviceTests = MultiRootDeviceFixture;

TEST_F(KernelInfoMultiRootDeviceTests, kernelAllocationHasCorrectRootDeviceIndex) {
    KernelInfo kernelInfo;
    SKernelBinaryHeaderCommon kernelHeader;
    const size_t heapSize = 0x40;
    char heap[heapSize];
    kernelHeader.KernelHeapSize = heapSize;
    kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
    kernelInfo.heapInfo.pKernelHeap = &heap;

    auto retVal = kernelInfo.createKernelAllocation(expectedRootDeviceIndex, mockMemoryManager);
    EXPECT_TRUE(retVal);
    auto allocation = kernelInfo.kernelAllocation;
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(expectedRootDeviceIndex, allocation->getRootDeviceIndex());
    mockMemoryManager->checkGpuUsageAndDestroyGraphicsAllocations(allocation);
}

TEST(KernelInfo, whenGetKernelNamesStringIsCalledThenNamesAreProperlyConcatenated) {
    ExecutionEnvironment execEnv;
    KernelInfo kernel1 = {};
    kernel1.name = "kern1";
    KernelInfo kernel2 = {};
    kernel2.name = "kern2";
    std::vector<KernelInfo *> kernelInfoArray;
    kernelInfoArray.push_back(&kernel1);
    kernelInfoArray.push_back(&kernel2);
    EXPECT_STREQ("kern1;kern2", concatenateKernelNames(kernelInfoArray).c_str());
}
