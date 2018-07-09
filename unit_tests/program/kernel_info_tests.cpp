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
    KernelInfo *pKernelInfo = KernelInfo::create();
    EXPECT_FALSE(pKernelInfo->usesSsh);
    EXPECT_FALSE(pKernelInfo->isValid);
    delete pKernelInfo;
}

TEST(KernelInfo, decodeConstantMemoryKernelArgument) {
    uint32_t argumentNumber = 0;
    KernelInfo *pKernelInfo = KernelInfo::create();
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

    delete pKernelInfo;
}

TEST(KernelInfo, decodeGlobalMemoryKernelArgument) {
    uint32_t argumentNumber = 1;
    KernelInfo *pKernelInfo = KernelInfo::create();
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

    delete pKernelInfo;
}

TEST(KernelInfo, decodeImageKernelArgument) {
    uint32_t argumentNumber = 1;
    KernelInfo *pKernelInfo = KernelInfo::create();
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

    delete pKernelInfo;
}

TEST(KernelInfoTest, givenKernelInfoWhenCreateKernelAllocationThenCopyWholeKernelHeapToKernelAllocation) {
    KernelInfo kernelInfo;
    OsAgnosticMemoryManager memoryManager;
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
    GraphicsAllocation *allocate32BitGraphicsMemory(size_t size, const void *ptr, AllocationOrigin allocationOrigin) override { return nullptr; }
};

TEST(KernelInfoTest, givenKernelInfoWhenCreateKernelAllocationAndCannotAllocateMemoryThenReturnsFalse) {
    KernelInfo kernelInfo;
    MyMemoryManager memoryManager;
    SKernelBinaryHeaderCommon kernelHeader;
    kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
    auto retVal = kernelInfo.createKernelAllocation(&memoryManager);
    EXPECT_FALSE(retVal);
}

TEST(KernelInfo, decodeGlobalMemObjectKernelArgument) {
    uint32_t argumentNumber = 1;
    KernelInfo *pKernelInfo = KernelInfo::create();
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

    delete pKernelInfo;
}

TEST(KernelInfo, decodeSamplerKernelArgument) {
    uint32_t argumentNumber = 1;
    KernelInfo *pKernelInfo = KernelInfo::create();
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
    delete pKernelInfo;
}

typedef KernelInfo KernelInfo_resolveKernelInfo;
TEST(KernelInfo_resolveKernelInfo, basicArgument) {
    KernelInfo *pKernelInfo = KernelInfo::create();

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

    delete pKernelInfo;
}

TEST(KernelInfo_resolveKernelInfo, complexArgumentType) {
    KernelInfo *pKernelInfo = KernelInfo::create();

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

    delete pKernelInfo;
}

TEST(KernelInfo, givenKernelInfoWhenStoreTransformableArgThenArgInfoIsTransformable) {
    uint32_t argumentNumber = 1;
    std::unique_ptr<KernelInfo> kernelInfo(KernelInfo::create());
    SPatchImageMemoryObjectKernelArgument arg;
    arg.ArgumentNumber = argumentNumber;
    arg.Transformable = true;

    kernelInfo->storeKernelArgument(&arg);
    const auto &argInfo = kernelInfo->kernelArgInfo[argumentNumber];
    EXPECT_TRUE(argInfo.isTransformable);
}

TEST(KernelInfo, givenKernelInfoWhenStoreNonTransformableArgThenArgInfoIsNotTransformable) {
    uint32_t argumentNumber = 1;
    std::unique_ptr<KernelInfo> kernelInfo(KernelInfo::create());
    SPatchImageMemoryObjectKernelArgument arg;
    arg.ArgumentNumber = argumentNumber;
    arg.Transformable = false;

    kernelInfo->storeKernelArgument(&arg);
    const auto &argInfo = kernelInfo->kernelArgInfo[argumentNumber];
    EXPECT_FALSE(argInfo.isTransformable);
}
