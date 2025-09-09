/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/xe_hpg_core/hw_info_xe_hpg_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/multicontext_ocl_aub_fixture.h"
#include "opencl/test/unit_test/fixtures/simple_arg_kernel_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

struct XeHpgCoreStatelessCompressionInSBA : public KernelAUBFixture<StatelessCopyKernelFixture>,
                                            public ::testing::Test,
                                            public ::testing::WithParamInterface<uint32_t /*EngineType*/> {
    void SetUp() override {
        debugManager.flags.EnableStatelessCompression.set(1);
        debugManager.flags.RenderCompressedBuffersEnabled.set(true);
        debugManager.flags.RenderCompressedImagesEnabled.set(true);
        debugManager.flags.EnableLocalMemory.set(true);
        debugManager.flags.NodeOrdinal.set(GetParam());
        debugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::builtin));
        KernelAUBFixture<StatelessCopyKernelFixture>::setUp();
        if (!device->getHardwareInfo().featureTable.flags.ftrLocalMemory) {
            GTEST_SKIP();
        }
    }

    void TearDown() override {
        KernelAUBFixture<StatelessCopyKernelFixture>::tearDown();
    }

    DebugManagerStateRestore debugRestorer;
    cl_int retVal = CL_SUCCESS;
};

XE_HPG_CORETEST_P(XeHpgCoreStatelessCompressionInSBA, GENERATEONLY_givenCompressedBuffersWhenStatelessAccessToLocalMemoryThenEnableCompressionInSBA) {
    const size_t bufferSize = 2048;
    uint8_t writePattern[bufferSize];
    std::fill(writePattern, writePattern + sizeof(writePattern), 1);

    device->getGpgpuCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto unCompressedBuffer = std::unique_ptr<Buffer>(Buffer::create(context, CL_MEM_UNCOMPRESSED_HINT_INTEL, bufferSize, nullptr, retVal));
    auto unCompressedAllocation = unCompressedBuffer->getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_EQ(AllocationType::buffer, unCompressedAllocation->getAllocationType());
    EXPECT_FALSE(unCompressedAllocation->isCompressionEnabled());
    EXPECT_EQ(MemoryPool::localMemory, unCompressedAllocation->getMemoryPool());

    auto compressedBuffer1 = std::unique_ptr<Buffer>(Buffer::create(context, CL_MEM_COMPRESSED_HINT_INTEL, bufferSize, nullptr, retVal));
    auto compressedAllocation1 = compressedBuffer1->getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_TRUE(compressedAllocation1->isCompressionEnabled());
    EXPECT_EQ(MemoryPool::localMemory, compressedAllocation1->getMemoryPool());
    EXPECT_TRUE(compressedAllocation1->getDefaultGmm()->isCompressionEnabled());

    auto compressedBuffer2 = std::unique_ptr<Buffer>(Buffer::create(context, CL_MEM_COMPRESSED_HINT_INTEL, bufferSize, nullptr, retVal));
    auto compressedAllocation2 = compressedBuffer2->getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_TRUE(compressedAllocation2->isCompressionEnabled());
    EXPECT_EQ(MemoryPool::localMemory, compressedAllocation2->getMemoryPool());
    EXPECT_TRUE(compressedAllocation2->getDefaultGmm()->isCompressionEnabled());

    retVal = pCmdQ->enqueueWriteBuffer(compressedBuffer1.get(), CL_FALSE, 0, bufferSize, writePattern, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = kernel->setArg(0, compressedBuffer1.get());
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = kernel->setArg(1, compressedBuffer2.get());
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    retVal = pCmdQ->enqueueKernel(kernel, 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->finish(false);

    expectNotEqualMemory<FamilyType>(AUBFixture::getGpuPointer(compressedAllocation1), writePattern, bufferSize);

    expectNotEqualMemory<FamilyType>(AUBFixture::getGpuPointer(compressedAllocation2), writePattern, bufferSize);

    retVal = pCmdQ->enqueueCopyBuffer(compressedBuffer2.get(), unCompressedBuffer.get(), 0, 0, bufferSize, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->finish(false);

    expectMemory<FamilyType>(AUBFixture::getGpuPointer(unCompressedAllocation), writePattern, bufferSize);
}

XE_HPG_CORETEST_P(XeHpgCoreStatelessCompressionInSBA, GENERATEONLY_givenCompressedDeviceMemoryWhenAccessedStatelesslyThenEnableCompressionInSBA) {
    const size_t bufferSize = 2048;
    uint8_t writePattern[bufferSize];
    std::fill(writePattern, writePattern + sizeof(writePattern), 1);

    device->getGpgpuCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto compressedDeviceMemAllocPtr1 = clDeviceMemAllocINTEL(context, device, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, compressedDeviceMemAllocPtr1);
    auto compressedDeviceMemAlloc1 = context->getSVMAllocsManager()->getSVMAllocs()->get(compressedDeviceMemAllocPtr1)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_NE(nullptr, compressedDeviceMemAlloc1);
    EXPECT_TRUE(compressedDeviceMemAlloc1->isCompressionEnabled());
    EXPECT_EQ(MemoryPool::localMemory, compressedDeviceMemAlloc1->getMemoryPool());
    EXPECT_TRUE(compressedDeviceMemAlloc1->getDefaultGmm()->isCompressionEnabled());

    auto compressedDeviceMemAllocPtr2 = clDeviceMemAllocINTEL(context, device, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, compressedDeviceMemAllocPtr2);
    auto compressedDeviceMemAlloc2 = context->getSVMAllocsManager()->getSVMAllocs()->get(compressedDeviceMemAllocPtr2)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_NE(nullptr, compressedDeviceMemAlloc2);
    EXPECT_TRUE(compressedDeviceMemAlloc2->isCompressionEnabled());
    EXPECT_EQ(MemoryPool::localMemory, compressedDeviceMemAlloc2->getMemoryPool());
    EXPECT_TRUE(compressedDeviceMemAlloc2->getDefaultGmm()->isCompressionEnabled());

    retVal = clEnqueueMemcpyINTEL(pCmdQ, true, compressedDeviceMemAllocPtr1, writePattern, bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArgSVMPointer(multiDeviceKernel.get(), 0, compressedDeviceMemAllocPtr1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArgSVMPointer(multiDeviceKernel.get(), 1, compressedDeviceMemAllocPtr2);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    retVal = clEnqueueNDRangeKernel(pCmdQ, multiDeviceKernel.get(), 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    expectNotEqualMemory<FamilyType>(compressedDeviceMemAllocPtr2, writePattern, bufferSize);

    expectNotEqualMemory<FamilyType>(compressedDeviceMemAllocPtr2, writePattern, bufferSize);

    retVal = clMemFreeINTEL(context, compressedDeviceMemAllocPtr1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context, compressedDeviceMemAllocPtr2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

XE_HPG_CORETEST_P(XeHpgCoreStatelessCompressionInSBA, givenUncompressibleBufferInHostMemoryWhenAccessedStatelesslyThenDisableCompressionInSBA) {
    const size_t bufferSize = 2048;
    uint8_t writePattern[bufferSize];
    std::fill(writePattern, writePattern + sizeof(writePattern), 1);

    device->getGpgpuCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto unCompressedBuffer = std::unique_ptr<Buffer>(Buffer::create(context, CL_MEM_UNCOMPRESSED_HINT_INTEL, bufferSize, nullptr, retVal));
    auto unCompressedAllocation = unCompressedBuffer->getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_EQ(AllocationType::buffer, unCompressedAllocation->getAllocationType());
    EXPECT_FALSE(unCompressedAllocation->isCompressionEnabled());
    EXPECT_EQ(MemoryPool::localMemory, unCompressedAllocation->getMemoryPool());

    auto compressedBuffer = std::unique_ptr<Buffer>(Buffer::create(context, CL_MEM_COMPRESSED_HINT_INTEL, bufferSize, nullptr, retVal));
    auto compressedAllocation = compressedBuffer->getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_TRUE(compressedAllocation->isCompressionEnabled());
    EXPECT_EQ(MemoryPool::localMemory, compressedAllocation->getMemoryPool());
    EXPECT_TRUE(compressedAllocation->getDefaultGmm()->isCompressionEnabled());

    auto uncompressibleBufferInHostMemory = std::unique_ptr<Buffer>(Buffer::create(context, CL_MEM_FORCE_HOST_MEMORY_INTEL, bufferSize, nullptr, retVal));
    auto uncompressibleAllocationInHostMemory = uncompressibleBufferInHostMemory->getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_EQ(AllocationType::bufferHostMemory, uncompressibleAllocationInHostMemory->getAllocationType());
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(uncompressibleAllocationInHostMemory->getMemoryPool()));

    retVal = pCmdQ->enqueueWriteBuffer(compressedBuffer.get(), CL_FALSE, 0, bufferSize, writePattern, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = kernel->setArg(0, compressedBuffer.get());
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = kernel->setArg(1, uncompressibleBufferInHostMemory.get());
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    retVal = pCmdQ->enqueueKernel(kernel, 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    pCmdQ->finish(false);

    expectNotEqualMemory<FamilyType>(AUBFixture::getGpuPointer(compressedAllocation), writePattern, bufferSize);

    expectMemory<FamilyType>(AUBFixture::getGpuPointer(uncompressibleAllocationInHostMemory), writePattern, bufferSize);

    retVal = pCmdQ->enqueueCopyBuffer(uncompressibleBufferInHostMemory.get(), unCompressedBuffer.get(), 0, 0, bufferSize, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->finish(false);

    expectMemory<FamilyType>(AUBFixture::getGpuPointer(unCompressedAllocation), writePattern, bufferSize);
}

XE_HPG_CORETEST_P(XeHpgCoreStatelessCompressionInSBA, givenUncompressibleHostMemoryAllocationWhenAccessedStatelesslyThenDisableCompressionInSBA) {
    const size_t bufferSize = 2048;
    uint8_t writePattern[bufferSize];
    std::fill(writePattern, writePattern + sizeof(writePattern), 1);

    device->getGpgpuCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto compressedDeviceMemAllocPtr = clDeviceMemAllocINTEL(context, device, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, compressedDeviceMemAllocPtr);
    auto compressedDeviceMemAlloc = context->getSVMAllocsManager()->getSVMAllocs()->get(compressedDeviceMemAllocPtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_NE(nullptr, compressedDeviceMemAlloc);
    EXPECT_TRUE(compressedDeviceMemAlloc->isCompressionEnabled());
    EXPECT_EQ(MemoryPool::localMemory, compressedDeviceMemAlloc->getMemoryPool());
    EXPECT_TRUE(compressedDeviceMemAlloc->getDefaultGmm()->isCompressionEnabled());

    auto uncompressibleHostMemAllocPtr = clHostMemAllocINTEL(context, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, uncompressibleHostMemAllocPtr);
    auto uncompressibleHostMemAlloc = context->getSVMAllocsManager()->getSVMAllocs()->get(uncompressibleHostMemAllocPtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_NE(nullptr, uncompressibleHostMemAlloc);
    EXPECT_EQ(AllocationType::bufferHostMemory, uncompressibleHostMemAlloc->getAllocationType());
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(uncompressibleHostMemAlloc->getMemoryPool()));

    retVal = clEnqueueMemcpyINTEL(pCmdQ, true, compressedDeviceMemAllocPtr, writePattern, bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArgSVMPointer(multiDeviceKernel.get(), 0, compressedDeviceMemAllocPtr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArgSVMPointer(multiDeviceKernel.get(), 1, uncompressibleHostMemAllocPtr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    retVal = clEnqueueNDRangeKernel(pCmdQ, multiDeviceKernel.get(), 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    expectNotEqualMemory<FamilyType>(compressedDeviceMemAllocPtr, writePattern, bufferSize);
    expectMemory<FamilyType>(uncompressibleHostMemAllocPtr, writePattern, bufferSize);

    retVal = clMemFreeINTEL(context, compressedDeviceMemAllocPtr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context, uncompressibleHostMemAllocPtr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

INSTANTIATE_TEST_SUITE_P(,
                         XeHpgCoreStatelessCompressionInSBA,
                         ::testing::Values(aub_stream::ENGINE_RCS,
                                           aub_stream::ENGINE_CCS));

struct XeHpgCoreUmStatelessCompressionInSBA : public KernelAUBFixture<StatelessKernelWithIndirectAccessFixture>,
                                              public ::testing::Test,
                                              public ::testing::WithParamInterface<uint32_t /*EngineType*/> {
    void SetUp() override {
        debugManager.flags.EnableStatelessCompression.set(1);
        debugManager.flags.RenderCompressedBuffersEnabled.set(true);
        debugManager.flags.RenderCompressedImagesEnabled.set(true);
        debugManager.flags.EnableLocalMemory.set(true);
        debugManager.flags.NodeOrdinal.set(GetParam());
        debugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::builtin));
        KernelAUBFixture<StatelessKernelWithIndirectAccessFixture>::setUp();
        if (!device->getHardwareInfo().featureTable.flags.ftrLocalMemory) {
            GTEST_SKIP();
        }
        EXPECT_TRUE(multiDeviceKernel->getKernel(rootDeviceIndex)->getKernelInfo().kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess);
    }

    void TearDown() override {
        KernelAUBFixture<StatelessKernelWithIndirectAccessFixture>::tearDown();
    }

    DebugManagerStateRestore debugRestorer;
    cl_int retVal = CL_SUCCESS;
};

XE_HPG_CORETEST_P(XeHpgCoreUmStatelessCompressionInSBA, GENERATEONLY_givenStatelessKernelWhenItHasIndirectDeviceAccessThenEnableCompressionInSBA) {
    const size_t bufferSize = MemoryConstants::kiloByte;
    uint8_t bufferData[bufferSize] = {};

    auto uncompressibleHostMemAlloc = clHostMemAllocINTEL(context, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, uncompressibleHostMemAlloc);

    auto compressedDeviceMemAlloc1 = clDeviceMemAllocINTEL(context, device, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, compressedDeviceMemAlloc1);

    auto compressedDeviceMemAlloc2 = clDeviceMemAllocINTEL(context, device, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, compressedDeviceMemAlloc2);

    retVal = clEnqueueMemcpyINTEL(pCmdQ, true, compressedDeviceMemAlloc2, bufferData, bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    reinterpret_cast<uint64_t *>(bufferData)[0] = reinterpret_cast<uint64_t>(compressedDeviceMemAlloc2);

    retVal = clEnqueueMemcpyINTEL(pCmdQ, true, compressedDeviceMemAlloc1, bufferData, bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArgSVMPointer(multiDeviceKernel.get(), 0, compressedDeviceMemAlloc1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelExecInfo(multiDeviceKernel.get(), CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL, sizeof(compressedDeviceMemAlloc2), &compressedDeviceMemAlloc2);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    retVal = clEnqueueNDRangeKernel(pCmdQ, multiDeviceKernel.get(), 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    reinterpret_cast<uint64_t *>(bufferData)[0] = 1;
    expectNotEqualMemory<FamilyType>(compressedDeviceMemAlloc2, bufferData, bufferSize);

    retVal = clEnqueueMemcpyINTEL(pCmdQ, true, uncompressibleHostMemAlloc, compressedDeviceMemAlloc2, bufferSize, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    expectMemory<FamilyType>(uncompressibleHostMemAlloc, bufferData, bufferSize);

    retVal = clMemFreeINTEL(context, uncompressibleHostMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context, compressedDeviceMemAlloc1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context, compressedDeviceMemAlloc2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

XE_HPG_CORETEST_P(XeHpgCoreUmStatelessCompressionInSBA, GENERATEONLY_givenKernelExecInfoWhenItHasIndirectDeviceAccessThenEnableCompressionInSBA) {
    const size_t bufferSize = MemoryConstants::kiloByte;
    uint8_t bufferData[bufferSize] = {};

    auto uncompressibleHostMemAlloc = clHostMemAllocINTEL(context, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, uncompressibleHostMemAlloc);

    auto compressedDeviceMemAlloc1 = clDeviceMemAllocINTEL(context, device, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, compressedDeviceMemAlloc1);

    auto compressedDeviceMemAlloc2 = clDeviceMemAllocINTEL(context, device, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, compressedDeviceMemAlloc2);

    retVal = clEnqueueMemcpyINTEL(pCmdQ, true, compressedDeviceMemAlloc2, bufferData, bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    reinterpret_cast<uint64_t *>(bufferData)[0] = reinterpret_cast<uint64_t>(compressedDeviceMemAlloc2);

    retVal = clEnqueueMemcpyINTEL(pCmdQ, true, compressedDeviceMemAlloc1, bufferData, bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArgSVMPointer(multiDeviceKernel.get(), 0, compressedDeviceMemAlloc1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_bool enableIndirectDeviceAccess = CL_TRUE;
    retVal = clSetKernelExecInfo(multiDeviceKernel.get(), CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectDeviceAccess);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    retVal = clEnqueueNDRangeKernel(pCmdQ, multiDeviceKernel.get(), 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    reinterpret_cast<uint64_t *>(bufferData)[0] = 1;
    expectNotEqualMemory<FamilyType>(compressedDeviceMemAlloc2, bufferData, bufferSize);

    retVal = clEnqueueMemcpyINTEL(pCmdQ, true, uncompressibleHostMemAlloc, compressedDeviceMemAlloc2, bufferSize, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    expectMemory<FamilyType>(uncompressibleHostMemAlloc, bufferData, bufferSize);

    retVal = clMemFreeINTEL(context, uncompressibleHostMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context, compressedDeviceMemAlloc1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context, compressedDeviceMemAlloc2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

XE_HPG_CORETEST_P(XeHpgCoreUmStatelessCompressionInSBA, givenStatelessKernelWhenItHasIndirectHostAccessThenDisableCompressionInSBA) {
    const size_t bufferSize = MemoryConstants::kiloByte;
    uint8_t bufferData[bufferSize] = {};

    auto compressedDeviceMemAlloc = clDeviceMemAllocINTEL(context, device, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, compressedDeviceMemAlloc);

    auto uncompressibleHostMemAlloc = clHostMemAllocINTEL(context, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, uncompressibleHostMemAlloc);

    memset(uncompressibleHostMemAlloc, 0, bufferSize);

    reinterpret_cast<uint64_t *>(bufferData)[0] = reinterpret_cast<uint64_t>(uncompressibleHostMemAlloc);

    retVal = clEnqueueMemcpyINTEL(pCmdQ, true, compressedDeviceMemAlloc, bufferData, bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArgSVMPointer(multiDeviceKernel.get(), 0, compressedDeviceMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelExecInfo(multiDeviceKernel.get(), CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL, sizeof(uncompressibleHostMemAlloc), &uncompressibleHostMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    retVal = clEnqueueNDRangeKernel(pCmdQ, multiDeviceKernel.get(), 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    reinterpret_cast<uint64_t *>(bufferData)[0] = 1;
    expectMemory<FamilyType>(uncompressibleHostMemAlloc, bufferData, bufferSize);

    retVal = clMemFreeINTEL(context, compressedDeviceMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context, uncompressibleHostMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

XE_HPG_CORETEST_P(XeHpgCoreUmStatelessCompressionInSBA, givenKernelExecInfoWhenItHasIndirectHostAccessThenDisableCompressionInSBA) {
    EXPECT_TRUE(multiDeviceKernel->getHasIndirectAccess());
    const size_t bufferSize = MemoryConstants::kiloByte;
    uint8_t bufferData[bufferSize] = {};

    auto compressedDeviceMemAlloc = clDeviceMemAllocINTEL(context, device, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, compressedDeviceMemAlloc);

    auto uncompressibleHostMemAlloc = clHostMemAllocINTEL(context, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, uncompressibleHostMemAlloc);

    memset(uncompressibleHostMemAlloc, 0, bufferSize);

    reinterpret_cast<uint64_t *>(bufferData)[0] = reinterpret_cast<uint64_t>(uncompressibleHostMemAlloc);

    retVal = clEnqueueMemcpyINTEL(pCmdQ, true, compressedDeviceMemAlloc, bufferData, bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArgSVMPointer(multiDeviceKernel.get(), 0, compressedDeviceMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_bool enableIndirectHostAccess = CL_TRUE;
    retVal = clSetKernelExecInfo(multiDeviceKernel.get(), CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectHostAccess);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    retVal = clEnqueueNDRangeKernel(pCmdQ, multiDeviceKernel.get(), 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    reinterpret_cast<uint64_t *>(bufferData)[0] = 1;
    expectMemory<FamilyType>(uncompressibleHostMemAlloc, bufferData, bufferSize);

    retVal = clMemFreeINTEL(context, compressedDeviceMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context, uncompressibleHostMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

INSTANTIATE_TEST_SUITE_P(,
                         XeHpgCoreUmStatelessCompressionInSBA,
                         ::testing::Values(aub_stream::ENGINE_RCS,
                                           aub_stream::ENGINE_CCS));

struct XeHpgCoreStatelessCompressionInSBAWithBCS : public MulticontextOclAubFixture,
                                                   public StatelessCopyKernelFixture,
                                                   public ::testing::Test {
    void SetUp() override {
        debugManager.flags.EnableStatelessCompression.set(1);
        debugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::blit));
        debugManager.flags.EnableBlitterOperationsSupport.set(true);
        MulticontextOclAubFixture::setUp(1, EnabledCommandStreamers::single, true);
        StatelessCopyKernelFixture::setUp(tileDevices[0], context.get());
        if (!tileDevices[0]->getHardwareInfo().featureTable.flags.ftrLocalMemory) {
            GTEST_SKIP();
        }
    }

    void TearDown() override {
        MulticontextOclAubFixture::tearDown();
        StatelessCopyKernelFixture::tearDown();
    }

    DebugManagerStateRestore debugRestorer;
    cl_int retVal = CL_SUCCESS;
};

XE_HPG_CORETEST_F(XeHpgCoreStatelessCompressionInSBAWithBCS, GENERATEONLY_givenCompressedBufferInDeviceMemoryWhenAccessedStatelesslyThenEnableCompressionInSBA) {
    const size_t bufferSize = 2048;
    uint8_t writePattern[bufferSize];
    std::fill(writePattern, writePattern + sizeof(writePattern), 1);

    auto unCompressedBuffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), CL_MEM_UNCOMPRESSED_HINT_INTEL, bufferSize, nullptr, retVal));
    auto unCompressedAllocation = unCompressedBuffer->getGraphicsAllocation(tileDevices[0]->getRootDeviceIndex());
    EXPECT_EQ(AllocationType::buffer, unCompressedAllocation->getAllocationType());
    EXPECT_EQ(MemoryPool::localMemory, unCompressedAllocation->getMemoryPool());

    auto compressedBuffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), CL_MEM_COMPRESSED_HINT_INTEL, bufferSize, nullptr, retVal));
    auto compressedAllocation = compressedBuffer->getGraphicsAllocation(tileDevices[0]->getRootDeviceIndex());
    EXPECT_TRUE(compressedAllocation->isCompressionEnabled());
    EXPECT_EQ(MemoryPool::localMemory, compressedAllocation->getMemoryPool());
    EXPECT_TRUE(compressedAllocation->getDefaultGmm()->isCompressionEnabled());

    retVal = commandQueues[0][0]->enqueueWriteBuffer(compressedBuffer.get(), CL_FALSE, 0, bufferSize, writePattern, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = kernel->setArg(0, compressedBuffer.get());
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = kernel->setArg(1, unCompressedBuffer.get());
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    retVal = commandQueues[0][0]->enqueueKernel(kernel, 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    commandQueues[0][0]->finish(false);

    expectMemoryNotEqual<FamilyType>(AUBFixture::getGpuPointer(compressedAllocation, compressedBuffer->getOffset()), writePattern, bufferSize, 0, 0);

    expectMemory<FamilyType>(AUBFixture::getGpuPointer(unCompressedAllocation, unCompressedBuffer->getOffset()), writePattern, bufferSize, 0, 0);
}

XE_HPG_CORETEST_F(XeHpgCoreStatelessCompressionInSBAWithBCS, givenUncompressibleBufferInHostMemoryWhenAccessedStatelesslyThenDisableCompressionInSBA) {
    const size_t bufferSize = 2048;
    uint8_t writePattern[bufferSize];
    std::fill(writePattern, writePattern + sizeof(writePattern), 1);

    auto compressedBuffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), CL_MEM_COMPRESSED_HINT_INTEL, bufferSize, nullptr, retVal));
    auto compressedAllocation = compressedBuffer->getGraphicsAllocation(tileDevices[0]->getRootDeviceIndex());
    EXPECT_TRUE(compressedAllocation->isCompressionEnabled());
    EXPECT_EQ(MemoryPool::localMemory, compressedAllocation->getMemoryPool());
    EXPECT_TRUE(compressedAllocation->getDefaultGmm()->isCompressionEnabled());

    auto uncompressibleBufferInHostMemory = std::unique_ptr<Buffer>(Buffer::create(context.get(), CL_MEM_FORCE_HOST_MEMORY_INTEL, bufferSize, nullptr, retVal));
    auto uncompressibleAllocationInHostMemory = uncompressibleBufferInHostMemory->getGraphicsAllocation(tileDevices[0]->getRootDeviceIndex());
    EXPECT_EQ(AllocationType::bufferHostMemory, uncompressibleAllocationInHostMemory->getAllocationType());
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(uncompressibleAllocationInHostMemory->getMemoryPool()));

    retVal = commandQueues[0][0]->enqueueWriteBuffer(compressedBuffer.get(), CL_FALSE, 0, bufferSize, writePattern, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = kernel->setArg(0, compressedBuffer.get());
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = kernel->setArg(1, uncompressibleBufferInHostMemory.get());
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    retVal = commandQueues[0][0]->enqueueKernel(kernel, 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    commandQueues[0][0]->finish(false);

    expectMemoryNotEqual<FamilyType>(AUBFixture::getGpuPointer(compressedAllocation, compressedBuffer->getOffset()), writePattern, bufferSize, 0, 0);

    expectMemory<FamilyType>(AUBFixture::getGpuPointer(uncompressibleAllocationInHostMemory, uncompressibleBufferInHostMemory->getOffset()), writePattern, bufferSize, 0, 0);
}
