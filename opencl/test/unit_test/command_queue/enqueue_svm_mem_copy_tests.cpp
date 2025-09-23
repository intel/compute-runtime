/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_align_malloc_memory_manager.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_builder.h"
#include "opencl/test/unit_test/mocks/mock_builtin_dispatch_info_builder.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

struct EnqueueSvmMemCopyTest : public ClDeviceFixture,
                               public CommandQueueHwFixture,
                               public ::testing::Test {
    typedef CommandQueueHwFixture CommandQueueFixture;

    EnqueueSvmMemCopyTest() {
    }

    void SetUp() override {
        ClDeviceFixture::setUp();

        if (!pDevice->isFullRangeSvm()) {
            return;
        }

        CommandQueueFixture::setUp(pClDevice, 0);
        srcSvmPtr = context->getSVMAllocsManager()->createSVMAlloc(256, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
        ASSERT_NE(nullptr, srcSvmPtr);
        dstSvmPtr = context->getSVMAllocsManager()->createSVMAlloc(256, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
        ASSERT_NE(nullptr, dstSvmPtr);
        auto srcSvmData = context->getSVMAllocsManager()->getSVMAlloc(srcSvmPtr);
        ASSERT_NE(nullptr, srcSvmData);
        srcSvmAlloc = srcSvmData->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
        ASSERT_NE(nullptr, srcSvmAlloc);
        auto dstSvmData = context->getSVMAllocsManager()->getSVMAlloc(dstSvmPtr);
        ASSERT_NE(nullptr, dstSvmData);
        dstSvmAlloc = dstSvmData->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
        ASSERT_NE(nullptr, dstSvmAlloc);

        auto &compilerProductHelper = pDevice->getCompilerProductHelper();
        this->useHeapless = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
        this->useStateless = compilerProductHelper.isForceToStatelessRequired();
    }

    void TearDown() override {
        if (pDevice->isFullRangeSvm()) {
            context->getSVMAllocsManager()->freeSVMAlloc(srcSvmPtr);
            context->getSVMAllocsManager()->freeSVMAlloc(dstSvmPtr);
            CommandQueueFixture::tearDown();
        }
        ClDeviceFixture::tearDown();
    }

    EBuiltInOps::Type getAdjustedCopyBufferToBufferBuiltIn() {
        if (useHeapless) {
            return EBuiltInOps::copyBufferToBufferStatelessHeapless;
        } else if (useStateless) {
            return EBuiltInOps::copyBufferToBufferStateless;
        }

        return EBuiltInOps::copyBufferToBuffer;
    }

    void *srcSvmPtr = nullptr;
    void *dstSvmPtr = nullptr;
    GraphicsAllocation *srcSvmAlloc = nullptr;
    GraphicsAllocation *dstSvmAlloc = nullptr;
    bool useHeapless = false;
    bool useStateless = false;
};

HWTEST_F(EnqueueSvmMemCopyTest, givenEnqueueSVMMemcpyWhenUsingCopyBufferToBufferBuilderThenItConfiguredWithBuiltinOpsAndProducesDispatchInfo) {
    if (!pDevice->isFullRangeSvm()) {
        return;
    }

    auto builtIn = getAdjustedCopyBufferToBufferBuiltIn();

    auto builtIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()].get(), builtIns);
    // retrieve original builder
    auto &origBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        builtIn,
        pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &origBuilder);

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        builtIn,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(*builtIns, pCmdQ->getClDevice(), &origBuilder)));
    EXPECT_EQ(&origBuilder, oldBuilder.get());

    // call enqueue on mock builder
    auto retVal = pCmdQ->enqueueSVMMemcpy(
        false,     // cl_bool  blocking_copy
        dstSvmPtr, // void *dst_ptr
        srcSvmPtr, // const void *src_ptr
        256,       // size_t size
        0,         // cl_uint num_events_in_wait_list
        nullptr,   // cl_event *event_wait_list
        nullptr,   // cL_event *event
        nullptr    // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        builtIn,
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);

    // check if original builder is restored correctly
    auto &restoredBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        builtIn,
        pCmdQ->getClDevice());
    EXPECT_EQ(&origBuilder, &restoredBuilder);

    // use mock builder to validate builder's input / output
    auto mockBuilder = static_cast<MockBuiltinDispatchInfoBuilder *>(newBuilder.get());

    // validate builder's input - builtin ops
    auto params = mockBuilder->getBuiltinOpParams();
    EXPECT_EQ(srcSvmPtr, params->srcPtr);
    EXPECT_EQ(dstSvmPtr, params->dstPtr);
    EXPECT_EQ(nullptr, params->srcMemObj);
    EXPECT_EQ(nullptr, params->dstMemObj);
    EXPECT_EQ(srcSvmAlloc, params->srcSvmAlloc);
    EXPECT_EQ(dstSvmAlloc, params->dstSvmAlloc);
    EXPECT_EQ(Vec3<size_t>(0, 0, 0), params->srcOffset);
    EXPECT_EQ(Vec3<size_t>(0, 0, 0), params->dstOffset);
    EXPECT_EQ(Vec3<size_t>(256, 0, 0), params->size);

    // validate builder's output - multi dispatch info
    auto mdi = mockBuilder->getMultiDispatchInfo();
    EXPECT_EQ(1u, mdi->size());

    auto di = mdi->begin();
    size_t middleElSize = 4 * sizeof(uint32_t);
    EXPECT_EQ(Vec3<size_t>(256 / middleElSize, 1, 1), di->getGWS());

    auto kernel = mdi->begin()->getKernel();
    EXPECT_EQ(EBuiltInOps::isStateless(builtIn) ? "CopyBufferToBufferMiddleStateless" : "CopyBufferToBufferMiddle", kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName);
}

HWTEST_F(EnqueueSvmMemCopyTest, givenEnqueueSVMMemcpyWhenUsingCopyBufferToBufferBuilderAndSrcHostPtrThenItConfiguredWithBuiltinOpsAndProducesDispatchInfo) {
    if (!pDevice->isFullRangeSvm()) {
        return;
    }

    auto builtIn = getAdjustedCopyBufferToBufferBuiltIn();

    auto builtIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()].get(), builtIns);
    void *srcHostPtr = alignedMalloc(512, 64);
    size_t hostPtrOffset = 2;

    // retrieve original builder
    auto &origBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        builtIn,
        pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &origBuilder);

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        builtIn,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(*builtIns, pCmdQ->getClDevice(), &origBuilder)));
    EXPECT_EQ(&origBuilder, oldBuilder.get());

    // call enqueue on mock builder
    auto retVal = pCmdQ->enqueueSVMMemcpy(
        false,                                // cl_bool  blocking_copy
        dstSvmPtr,                            // void *dst_ptr
        ptrOffset(srcHostPtr, hostPtrOffset), // const void *src_ptr
        256,                                  // size_t size
        0,                                    // cl_uint num_events_in_wait_list
        nullptr,                              // cl_event *event_wait_list
        nullptr,                              // cL_event *event
        nullptr                               // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        builtIn,
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);

    // check if original builder is restored correctly
    auto &restoredBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        builtIn,
        pCmdQ->getClDevice());
    EXPECT_EQ(&origBuilder, &restoredBuilder);

    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    GraphicsAllocation *srcSvmAlloc = nullptr;

    auto head = ultCsr.getTemporaryAllocations().peekHead();
    while (head) {
        if (ptrOffset(srcHostPtr, hostPtrOffset) == head->getUnderlyingBuffer()) {
            srcSvmAlloc = head;
            break;
        }
        head = head->next;
    }
    EXPECT_NE(nullptr, srcSvmAlloc);

    // use mock builder to validate builder's input / output
    auto mockBuilder = static_cast<MockBuiltinDispatchInfoBuilder *>(newBuilder.get());

    // validate builder's input - builtin ops
    auto params = mockBuilder->getBuiltinOpParams();
    EXPECT_EQ(alignDown(srcSvmAlloc->getGpuAddress(), 4), castToUint64(params->srcPtr));
    EXPECT_EQ(dstSvmPtr, params->dstPtr);
    EXPECT_EQ(nullptr, params->srcMemObj);
    EXPECT_EQ(nullptr, params->dstMemObj);
    EXPECT_EQ(srcSvmAlloc, params->srcSvmAlloc);
    EXPECT_EQ(dstSvmAlloc, params->dstSvmAlloc);
    EXPECT_EQ(Vec3<size_t>(2, 0, 0), params->srcOffset);
    EXPECT_EQ(Vec3<size_t>(0, 0, 0), params->dstOffset);
    EXPECT_EQ(Vec3<size_t>(256, 0, 0), params->size);

    alignedFree(srcHostPtr);
}

HWTEST_F(EnqueueSvmMemCopyTest, givenEnqueueSVMMemcpyWhenUsingCopyBufferToBufferBuilderAndDstHostPtrThenItConfiguredWithBuiltinOpsAndProducesDispatchInfo) {
    if (!pDevice->isFullRangeSvm()) {
        return;
    }

    auto builtIn = getAdjustedCopyBufferToBufferBuiltIn();

    auto builtIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()].get(), builtIns);
    auto dstHostPtr = alignedMalloc(512, 64);
    size_t hostPtrOffset = 2;

    // retrieve original builder
    auto &origBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        builtIn,
        pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &origBuilder);

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        builtIn,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(*builtIns, pCmdQ->getClDevice(), &origBuilder)));
    EXPECT_EQ(&origBuilder, oldBuilder.get());

    // call enqueue on mock builder
    auto retVal = pCmdQ->enqueueSVMMemcpy(
        false,                                // cl_bool  blocking_copy
        ptrOffset(dstHostPtr, hostPtrOffset), // void *dst_ptr
        srcSvmPtr,                            // const void *src_ptr
        256,                                  // size_t size
        0,                                    // cl_uint num_events_in_wait_list
        nullptr,                              // cl_event *event_wait_list
        nullptr,                              // cL_event *event
        nullptr                               // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        builtIn,
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);

    // check if original builder is restored correctly
    auto &restoredBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        builtIn,
        pCmdQ->getClDevice());
    EXPECT_EQ(&origBuilder, &restoredBuilder);

    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    GraphicsAllocation *dstSvmAlloc = nullptr;

    auto head = ultCsr.getTemporaryAllocations().peekHead();
    while (head) {
        if (ptrOffset(dstHostPtr, hostPtrOffset) == head->getUnderlyingBuffer()) {
            dstSvmAlloc = head;
            break;
        }
        head = head->next;
    }
    EXPECT_NE(nullptr, dstSvmAlloc);

    // use mock builder to validate builder's input / output
    auto mockBuilder = static_cast<MockBuiltinDispatchInfoBuilder *>(newBuilder.get());

    // validate builder's input - builtin ops
    auto params = mockBuilder->getBuiltinOpParams();
    EXPECT_EQ(srcSvmPtr, params->srcPtr);
    EXPECT_EQ(alignDown(dstSvmAlloc->getGpuAddress(), 4), castToUint64(params->dstPtr));
    EXPECT_EQ(nullptr, params->srcMemObj);
    EXPECT_EQ(nullptr, params->dstMemObj);
    EXPECT_EQ(srcSvmAlloc, params->srcSvmAlloc);
    EXPECT_EQ(dstSvmAlloc, params->dstSvmAlloc);
    EXPECT_EQ(Vec3<size_t>(0, 0, 0), params->srcOffset);
    EXPECT_EQ(Vec3<size_t>(2, 0, 0), params->dstOffset);
    EXPECT_EQ(Vec3<size_t>(256, 0, 0), params->size);

    alignedFree(dstHostPtr);
}

HWTEST_F(EnqueueSvmMemCopyTest, givenCommandQueueWhenEnqueueSVMMemcpyIsCalledThenSetAllocDumpable) {
    if (!pDevice->isFullRangeSvm()) {
        return;
    }

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.AUBDumpAllocsOnEnqueueSVMMemcpyOnly.set(true);
    debugManager.flags.AUBDumpBufferFormat.set("BIN");

    auto dstHostPtr = alignedMalloc(256, 64);

    EXPECT_FALSE(srcSvmAlloc->isAllocDumpable());

    auto retVal = pCmdQ->enqueueSVMMemcpy(
        CL_TRUE,    // cl_bool  blocking_copy
        dstHostPtr, // void *dst_ptr
        srcSvmPtr,  // const void *src_ptr
        256,        // size_t size
        0,          // cl_uint num_events_in_wait_list
        nullptr,    // cl_event *event_wait_list
        nullptr,    // cL_event *event
        nullptr     // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(srcSvmAlloc->isAllocDumpable());

    alignedFree(dstHostPtr);
}

HWTEST_F(EnqueueSvmMemCopyTest, givenCommandQueueWhenEnqueueSVMMemcpyIsCalledThenItCallsNotifyFunction) {
    if (!pDevice->isFullRangeSvm()) {
        return;
    }

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    auto dstHostPtr = alignedMalloc(256, 64);

    auto retVal = mockCmdQ->enqueueSVMMemcpy(
        CL_TRUE,    // cl_bool  blocking_copy
        dstHostPtr, // void *dst_ptr
        srcSvmPtr,  // const void *src_ptr
        256,        // size_t size
        0,          // cl_uint num_events_in_wait_list
        nullptr,    // cl_event *event_wait_list
        nullptr,    // cL_event *event
        nullptr     // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockCmdQ->notifyEnqueueSVMMemcpyCalled);

    MultiGraphicsAllocation &srcSvmAlloc = context->getSVMAllocsManager()->getSVMAlloc(srcSvmPtr)->gpuAllocations;
    CsrSelectionArgs csrSelectionArgs{CL_COMMAND_SVM_MEMCPY, &srcSvmAlloc, {}, 0, nullptr};
    CommandStreamReceiver &csr = mockCmdQ->selectCsrForBuiltinOperation(csrSelectionArgs);
    EXPECT_EQ(EngineHelpers::isBcs(csr.getOsContext().getEngineType()), mockCmdQ->useBcsCsrOnNotifyEnabled);

    alignedFree(dstHostPtr);
}

HWTEST_F(EnqueueSvmMemCopyTest, givenCsrSelectionArgsCalledWithRootDeviceIndexGreaterThanNumAllocationsThenNoSourceResourceSet) {
    if (!pDevice->isFullRangeSvm()) {
        return;
    }

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    auto dstHostPtr = alignedMalloc(256, 64);

    auto retVal = mockCmdQ->enqueueSVMMemcpy(
        CL_TRUE,    // cl_bool  blocking_copy
        dstHostPtr, // void *dst_ptr
        srcSvmPtr,  // const void *src_ptr
        256,        // size_t size
        0,          // cl_uint num_events_in_wait_list
        nullptr,    // cl_event *event_wait_list
        nullptr,    // cL_event *event
        nullptr     // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockCmdQ->notifyEnqueueSVMMemcpyCalled);

    MultiGraphicsAllocation &srcSvmAlloc = context->getSVMAllocsManager()->getSVMAlloc(srcSvmPtr)->gpuAllocations;
    CsrSelectionArgs csrSelectionArgs{CL_COMMAND_SVM_MEMCPY, &srcSvmAlloc, {}, static_cast<uint32_t>(srcSvmAlloc.getGraphicsAllocations().size()) + 1, nullptr};
    EXPECT_EQ(csrSelectionArgs.srcResource.allocation, nullptr);

    alignedFree(dstHostPtr);
}

HWTEST_F(EnqueueSvmMemCopyTest, givenConstHostMemoryAsSourceWhenEnqueueSVMMemcpyThenCpuCopyIsAllowed) {
    if (!pDevice->isFullRangeSvm()) {
        GTEST_SKIP();
    }

    constexpr double srcConstHostPtr[] = {42.0};

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

    auto retVal = mockCmdQ->enqueueSVMMemcpy(
        CL_TRUE,         // cl_bool  blocking_copy
        dstSvmPtr,       // void *dst_ptr
        srcConstHostPtr, // const void *src_ptr
        sizeof(double),  // size_t size
        0,               // cl_uint num_events_in_wait_list
        nullptr,         // cl_event *event_wait_list
        nullptr,         // cL_event *event
        nullptr          // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &ultCSR = mockCmdQ->getUltCommandStreamReceiver();
    EXPECT_GT(ultCSR.createAllocationForHostSurfaceCalled, 0u);
    EXPECT_TRUE(ultCSR.cpuCopyForHostPtrSurfaceAllowed);
}

struct EnqueueSvmMemCopyHw : public ::testing::Test {

    void SetUp() override {

        device = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        if (is32bit || !device->isFullRangeSvm()) {
            GTEST_SKIP();
        }

        context = std::make_unique<MockContext>(device.get());
        srcSvmPtr = context->getSVMAllocsManager()->createSVMAlloc(256, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
        ASSERT_NE(nullptr, srcSvmPtr);
        dstHostPtr = alignedMalloc(256, 64);
    }

    void TearDown() override {
        if (is32bit || !device->isFullRangeSvm()) {
            return;
        }
        context->getSVMAllocsManager()->freeSVMAlloc(srcSvmPtr);
        alignedFree(dstHostPtr);
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    uint64_t bigSize = 5ull * MemoryConstants::gigaByte;
    uint64_t smallSize = 4ull * MemoryConstants::gigaByte - 1;
    void *srcSvmPtr = nullptr;
    void *dstHostPtr = nullptr;
};

using EnqueueSvmMemCopyHwTest = EnqueueSvmMemCopyHw;

HWTEST_F(EnqueueSvmMemCopyHwTest, givenEnqueueSVMMemCopyWhenUsingCopyBufferToBufferStatelessBuilderThenSuccessIsReturned) {
    auto cmdQ = std::make_unique<CommandQueueStateless<FamilyType>>(context.get(), device.get());
    auto srcSvmData = context->getSVMAllocsManager()->getSVMAlloc(srcSvmPtr);
    srcSvmData->size = static_cast<size_t>(bigSize);
    auto retVal = cmdQ->enqueueSVMMemcpy(
        false,                        // cl_bool  blocking_copy
        dstHostPtr,                   // void *dst_ptr
        srcSvmPtr,                    // const void *src_ptr
        static_cast<size_t>(bigSize), // size_t size
        0,                            // cl_uint num_events_in_wait_list
        nullptr,                      // cl_event *event_wait_list
        nullptr,                      // cL_event *event
        nullptr                       // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST2_F(EnqueueSvmMemCopyHwTest, givenEnqueueSVMMemCopyWhenUsingCopyBufferToBufferStatefulBuilderThenSuccessIsReturned, IsStatefulBufferPreferredForProduct) {
    auto cmdQ = std::make_unique<CommandQueueStateful<FamilyType>>(context.get(), device.get());
    if (cmdQ->getHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    auto retVal = cmdQ->enqueueSVMMemcpy(
        false,                          // cl_bool  blocking_copy
        dstHostPtr,                     // void *dst_ptr
        srcSvmPtr,                      // const void *src_ptr
        static_cast<size_t>(smallSize), // size_t size
        0,                              // cl_uint num_events_in_wait_list
        nullptr,                        // cl_event *event_wait_list
        nullptr,                        // cL_event *event
        nullptr                         // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueSvmMemCopyTest, givenEnqueueSvmMemcpyWhenSvmZeroCopyThenBuiltinKernelUsesSystemMemory) {
    if (!pDevice->isFullRangeSvm()) {
        return;
    }

    auto builtIn = getAdjustedCopyBufferToBufferBuiltIn();

    auto builtIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()].get(), builtIns);
    // retrieve original builder
    auto &origBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        builtIn,
        pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &origBuilder);

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        builtIn,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(*builtIns, pCmdQ->getClDevice(), &origBuilder)));
    EXPECT_EQ(&origBuilder, oldBuilder.get());

    srcSvmAlloc->setAllocationType(NEO::AllocationType::svmZeroCopy);
    dstSvmAlloc->setAllocationType(NEO::AllocationType::svmZeroCopy);

    // call enqueue on mock builder
    auto retVal = pCmdQ->enqueueSVMMemcpy(
        false,     // cl_bool  blocking_copy
        dstSvmPtr, // void *dst_ptr
        srcSvmPtr, // const void *src_ptr
        256,       // size_t size
        0,         // cl_uint num_events_in_wait_list
        nullptr,   // cl_event *event_wait_list
        nullptr,   // cL_event *event
        nullptr    // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        builtIn,
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);

    // check if original builder is restored correctly
    auto &restoredBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        builtIn,
        pCmdQ->getClDevice());
    EXPECT_EQ(&origBuilder, &restoredBuilder);

    // use mock builder to validate builder's input / output
    auto mockBuilder = static_cast<MockBuiltinDispatchInfoBuilder *>(newBuilder.get());

    // validate builder's input - builtin ops
    auto params = mockBuilder->getBuiltinOpParams();
    EXPECT_EQ(srcSvmPtr, params->srcPtr);
    EXPECT_EQ(dstSvmPtr, params->dstPtr);
    EXPECT_EQ(nullptr, params->srcMemObj);
    EXPECT_EQ(nullptr, params->dstMemObj);
    EXPECT_EQ(srcSvmAlloc, params->srcSvmAlloc);
    EXPECT_EQ(dstSvmAlloc, params->dstSvmAlloc);
    EXPECT_EQ(Vec3<size_t>(0, 0, 0), params->srcOffset);
    EXPECT_EQ(Vec3<size_t>(0, 0, 0), params->dstOffset);
    EXPECT_EQ(Vec3<size_t>(256, 0, 0), params->size);

    // validate builder's output - multi dispatch info
    auto mdi = mockBuilder->getMultiDispatchInfo();
    EXPECT_EQ(1u, mdi->size());

    auto di = mdi->begin();
    size_t middleElSize = 4 * sizeof(uint32_t);
    EXPECT_EQ(Vec3<size_t>(256 / middleElSize, 1, 1), di->getGWS());

    auto kernel = mdi->begin()->getKernel();
    EXPECT_TRUE(kernel->getDestinationAllocationInSystemMemory());
}

HWTEST_F(EnqueueSvmMemCopyTest, givenEnqueueSvmMemcpyWhenSvmGpuThenBuiltinKernelNotUsesSystemMemory) {
    if (!pDevice->isFullRangeSvm()) {
        return;
    }

    auto builtIn = getAdjustedCopyBufferToBufferBuiltIn();

    auto builtIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()].get(), builtIns);
    // retrieve original builder
    auto &origBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        builtIn,
        pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &origBuilder);

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        builtIn,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(*builtIns, pCmdQ->getClDevice(), &origBuilder)));
    EXPECT_EQ(&origBuilder, oldBuilder.get());

    srcSvmAlloc->setAllocationType(NEO::AllocationType::svmGpu);
    dstSvmAlloc->setAllocationType(NEO::AllocationType::svmGpu);

    // call enqueue on mock builder
    auto retVal = pCmdQ->enqueueSVMMemcpy(
        false,     // cl_bool  blocking_copy
        dstSvmPtr, // void *dst_ptr
        srcSvmPtr, // const void *src_ptr
        256,       // size_t size
        0,         // cl_uint num_events_in_wait_list
        nullptr,   // cl_event *event_wait_list
        nullptr,   // cL_event *event
        nullptr    // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        builtIn,
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);

    // check if original builder is restored correctly
    auto &restoredBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        builtIn,
        pCmdQ->getClDevice());
    EXPECT_EQ(&origBuilder, &restoredBuilder);

    // use mock builder to validate builder's input / output
    auto mockBuilder = static_cast<MockBuiltinDispatchInfoBuilder *>(newBuilder.get());

    // validate builder's input - builtin ops
    auto params = mockBuilder->getBuiltinOpParams();
    EXPECT_EQ(srcSvmPtr, params->srcPtr);
    EXPECT_EQ(dstSvmPtr, params->dstPtr);
    EXPECT_EQ(nullptr, params->srcMemObj);
    EXPECT_EQ(nullptr, params->dstMemObj);
    EXPECT_EQ(srcSvmAlloc, params->srcSvmAlloc);
    EXPECT_EQ(dstSvmAlloc, params->dstSvmAlloc);
    EXPECT_EQ(Vec3<size_t>(0, 0, 0), params->srcOffset);
    EXPECT_EQ(Vec3<size_t>(0, 0, 0), params->dstOffset);
    EXPECT_EQ(Vec3<size_t>(256, 0, 0), params->size);

    // validate builder's output - multi dispatch info
    auto mdi = mockBuilder->getMultiDispatchInfo();
    EXPECT_EQ(1u, mdi->size());

    auto di = mdi->begin();
    size_t middleElSize = 4 * sizeof(uint32_t);
    EXPECT_EQ(Vec3<size_t>(256 / middleElSize, 1, 1), di->getGWS());

    auto kernel = mdi->begin()->getKernel();
    EXPECT_FALSE(kernel->getDestinationAllocationInSystemMemory());
}

struct StatelessMockAlignedMallocMemoryManagerEnqueueSvmMemCopyTest : public EnqueueSvmMemCopyTest {
    void SetUp() override {
        if (is32bit) {
            GTEST_SKIP();
        }
        EnqueueSvmMemCopyTest::SetUp();

        mockSvmManager = reinterpret_cast<MockSVMAllocsManager *>(context->getSVMAllocsManager());

        alignedMemoryManager = std::make_unique<MockAlignMallocMemoryManager>(*pClExecutionEnvironment, true);

        memoryManagerBackup = mockSvmManager->memoryManager;
        mockSvmManager->memoryManager = alignedMemoryManager.get();

        srcSvmPtr4gb = mockSvmManager->createSVMAlloc(static_cast<size_t>(4ull * MemoryConstants::gigaByte), {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
        EXPECT_NE(srcSvmPtr4gb, nullptr);
        dstSvmPtr4gb = mockSvmManager->createSVMAlloc(static_cast<size_t>(4ull * MemoryConstants::gigaByte), {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
        EXPECT_NE(srcSvmPtr4gb, nullptr);
    }

    void TearDown() override {
        if (is32bit) {
            GTEST_SKIP();
        }

        mockSvmManager->freeSVMAlloc(srcSvmPtr4gb);
        mockSvmManager->freeSVMAlloc(dstSvmPtr4gb);

        mockSvmManager->memoryManager = memoryManagerBackup;
        EnqueueSvmMemCopyTest::TearDown();
    }

  protected:
    MockSVMAllocsManager *mockSvmManager = nullptr;
    void *srcSvmPtr4gb = nullptr;
    void *dstSvmPtr4gb = nullptr;

  private:
    std::unique_ptr<MockAlignMallocMemoryManager> alignedMemoryManager;
    MemoryManager *memoryManagerBackup = nullptr;
};

HWTEST_F(StatelessMockAlignedMallocMemoryManagerEnqueueSvmMemCopyTest, given4gbBuffersAndIsForceStatelessIsFalseWhenEnqueueSvmMemcpyCallThenStatelessIsUsed) {
    static_cast<MockCommandQueueHw<FamilyType> *>(pCmdQ)->isForceStateless = false;

    EBuiltInOps::Type copyBuiltIn = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(true, pCmdQ->getHeaplessModeEnabled());

    auto builtIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()].get(), builtIns);

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuilder(*builtIns, pCmdQ->getClDevice())));

    auto mockBuilder = static_cast<MockBuilder *>(&BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        copyBuiltIn,
        *pClDevice));

    EXPECT_FALSE(mockBuilder->wasBuildDispatchInfosWithBuiltinOpParamsCalled);
    // call enqueue on mock builder
    auto retVal = pCmdQ->enqueueSVMMemcpy(
        false,
        dstSvmPtr4gb,
        srcSvmPtr4gb,
        256,
        0,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockBuilder->wasBuildDispatchInfosWithBuiltinOpParamsCalled);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);
}

HWTEST_F(StatelessMockAlignedMallocMemoryManagerEnqueueSvmMemCopyTest, givenDst4gbBufferAndSrcSmallBufferAndIsForceStatelessIsFalseWhenEnqueueSvmMemcpyCallThenStatelessIsUsed) {
    static_cast<MockCommandQueueHw<FamilyType> *>(pCmdQ)->isForceStateless = false;

    EBuiltInOps::Type copyBuiltIn = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(true, pCmdQ->getHeaplessModeEnabled());

    auto builtIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()].get(), builtIns);

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuilder(*builtIns, pCmdQ->getClDevice())));

    auto mockBuilder = static_cast<MockBuilder *>(&BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        copyBuiltIn,
        *pClDevice));

    EXPECT_FALSE(mockBuilder->wasBuildDispatchInfosWithBuiltinOpParamsCalled);
    // call enqueue on mock builder
    auto retVal = pCmdQ->enqueueSVMMemcpy(
        false,
        dstSvmPtr4gb,
        srcSvmPtr,
        256,
        0,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockBuilder->wasBuildDispatchInfosWithBuiltinOpParamsCalled);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);
}
