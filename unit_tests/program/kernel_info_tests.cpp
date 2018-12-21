/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/program/kernel_info.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "gtest/gtest.h"
#include <type_traits>
#include <memory>

using namespace OCLRT;

TEST(KernelInfo, NonCopyable) {
    EXPECT_FALSE(std::is_move_constructible<KernelInfo>::value);
    EXPECT_FALSE(std::is_copy_constructible<KernelInfo>::value);
}

TEST(KernelInfo, NonAssignable) {
    EXPECT_FALSE(std::is_move_assignable<KernelInfo>::value);
    EXPECT_FALSE(std::is_copy_assignable<KernelInfo>::value);
}

TEST(KernelInfo, defaultBehavior) {
    auto pKernelInfo = std::make_unique<KernelInfo>();
    EXPECT_FALSE(pKernelInfo->usesSsh);
    EXPECT_FALSE(pKernelInfo->isValid);
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
    //EXPECT_EQ(???, argInfo.argSize);
    EXPECT_EQ(arg.Offset, argInfo.offsetHeap);
    EXPECT_TRUE(argInfo.isImage);
    EXPECT_EQ(static_cast<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_READ_WRITE), argInfo.accessQualifier);
    //EXPECT_EQ(CL_KERNEL_ARG_ACCESS_READ_WRITE, argInfo.accessQualifier);
    //EXPECT_EQ(CL_KERNEL_ARG_ADDRESS_, argInfo.addressQualifier);
    //EXPECT_EQ(CL_KERNEL_ARG_TYPE_NONE, argInfo.typeQualifier);
}

TEST(KernelInfoTest, givenKernelInfoWhenCreateKernelAllocationThenCopyWholeKernelHeapToKernelAllocation) {
    KernelInfo kernelInfo;
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    SKernelBinaryHeaderCommon kernelHeader;
    const size_t heapSize = 0x40;
    char heap[heapSize];
    kernelHeader.KernelHeapSize = heapSize;
    kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
    kernelInfo.heapInfo.pKernelHeap = &heap;

    for (size_t i = 0; i < heapSize; i++) {
        heap[i] = static_cast<char>(i);
    }

    auto retVal = kernelInfo.createKernelAllocation(&memoryManager);
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
    ExecutionEnvironment executionEnvironment;
    MyMemoryManager memoryManager(false, false, executionEnvironment);
    SKernelBinaryHeaderCommon kernelHeader;
    kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
    auto retVal = kernelInfo.createKernelAllocation(&memoryManager);
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

typedef KernelInfo KernelInfo_resolveKernelInfo;
TEST(KernelInfo_resolveKernelInfo, basicArgument) {
    auto pKernelInfo = std::make_unique<KernelInfo>();

    pKernelInfo->kernelArgInfo.resize(1);
    auto &kernelArgInfo = pKernelInfo->kernelArgInfo[0];
    kernelArgInfo.accessQualifierStr = "read_only";
    kernelArgInfo.addressQualifierStr = "__global";
    kernelArgInfo.typeQualifierStr = "restrict";

    auto retVal = pKernelInfo->resolveKernelInfo();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(static_cast<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_READ_ONLY), kernelArgInfo.accessQualifier);
    EXPECT_EQ(static_cast<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_GLOBAL), kernelArgInfo.addressQualifier);
    EXPECT_EQ(static_cast<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_RESTRICT), kernelArgInfo.typeQualifier);
}

TEST(KernelInfo_resolveKernelInfo, complexArgumentType) {
    auto pKernelInfo = std::make_unique<KernelInfo>();

    pKernelInfo->kernelArgInfo.resize(1);
    auto &kernelArgInfo = pKernelInfo->kernelArgInfo[0];
    kernelArgInfo.accessQualifierStr = "read_only";
    kernelArgInfo.addressQualifierStr = "__global";
    kernelArgInfo.typeQualifierStr = "restrict const";

    auto retVal = pKernelInfo->resolveKernelInfo();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(static_cast<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_READ_ONLY), kernelArgInfo.accessQualifier);
    EXPECT_EQ(static_cast<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_GLOBAL), kernelArgInfo.addressQualifier);
    EXPECT_EQ(static_cast<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_RESTRICT | CL_KERNEL_ARG_TYPE_CONST), kernelArgInfo.typeQualifier);
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
