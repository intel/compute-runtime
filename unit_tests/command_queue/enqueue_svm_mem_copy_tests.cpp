/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/allocations_list.h"
#include "core/memory_manager/unified_memory_manager.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "test.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_builtin_dispatch_info_builder.h"
#include "unit_tests/mocks/mock_builtins.h"

using namespace NEO;

struct EnqueueSvmMemCopyTest : public DeviceFixture,
                               public CommandQueueHwFixture,
                               public ::testing::Test {
    typedef CommandQueueHwFixture CommandQueueFixture;

    EnqueueSvmMemCopyTest() {
    }

    void SetUp() override {
        DeviceFixture::SetUp();

        if (!pDevice->isFullRangeSvm()) {
            return;
        }

        CommandQueueFixture::SetUp(pClDevice, 0);
        srcSvmPtr = context->getSVMAllocsManager()->createSVMAlloc(pDevice->getRootDeviceIndex(), 256, {});
        ASSERT_NE(nullptr, srcSvmPtr);
        dstSvmPtr = context->getSVMAllocsManager()->createSVMAlloc(pDevice->getRootDeviceIndex(), 256, {});
        ASSERT_NE(nullptr, dstSvmPtr);
        auto srcSvmData = context->getSVMAllocsManager()->getSVMAlloc(srcSvmPtr);
        ASSERT_NE(nullptr, srcSvmData);
        srcSvmAlloc = srcSvmData->gpuAllocation;
        ASSERT_NE(nullptr, srcSvmAlloc);
        auto dstSvmData = context->getSVMAllocsManager()->getSVMAlloc(dstSvmPtr);
        ASSERT_NE(nullptr, dstSvmData);
        dstSvmAlloc = dstSvmData->gpuAllocation;
        ASSERT_NE(nullptr, dstSvmAlloc);
    }

    void TearDown() override {
        if (pDevice->isFullRangeSvm()) {
            context->getSVMAllocsManager()->freeSVMAlloc(srcSvmPtr);
            context->getSVMAllocsManager()->freeSVMAlloc(dstSvmPtr);
            CommandQueueFixture::TearDown();
        }
        DeviceFixture::TearDown();
    }

    void *srcSvmPtr = nullptr;
    void *dstSvmPtr = nullptr;
    GraphicsAllocation *srcSvmAlloc = nullptr;
    GraphicsAllocation *dstSvmAlloc = nullptr;
};

HWTEST_F(EnqueueSvmMemCopyTest, givenEnqueueSVMMemcpyWhenUsingCopyBufferToBufferBuilderThenItConfiguredWithBuiltinOpsAndProducesDispatchInfo) {
    if (!pDevice->isFullRangeSvm()) {
        return;
    }

    auto builtIns = new MockBuiltins();
    pCmdQ->getDevice().getExecutionEnvironment()->builtins.reset(builtIns);
    // retrieve original builder
    auto &origBuilder = builtIns->getBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice());
    ASSERT_NE(nullptr, &origBuilder);

    // substitute original builder with mock builder
    auto oldBuilder = builtIns->setBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(*builtIns, &origBuilder)));
    EXPECT_EQ(&origBuilder, oldBuilder.get());

    // call enqueue on mock builder
    auto retVal = pCmdQ->enqueueSVMMemcpy(
        false,     // cl_bool  blocking_copy
        dstSvmPtr, // void *dst_ptr
        srcSvmPtr, // const void *src_ptr
        256,       // size_t size
        0,         // cl_uint num_events_in_wait_list
        nullptr,   // cl_event *event_wait_list
        nullptr    // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    // restore original builder and retrieve mock builder
    auto newBuilder = builtIns->setBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);

    // check if original builder is restored correctly
    auto &restoredBuilder = builtIns->getBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice());
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
    EXPECT_EQ("CopyBufferToBufferMiddle", kernel->getKernelInfo().name);
}

HWTEST_F(EnqueueSvmMemCopyTest, givenEnqueueSVMMemcpyWhenUsingCopyBufferToBufferBuilderAndSrcHostPtrThenItConfiguredWithBuiltinOpsAndProducesDispatchInfo) {
    if (!pDevice->isFullRangeSvm()) {
        return;
    }

    auto builtIns = new MockBuiltins();
    pCmdQ->getDevice().getExecutionEnvironment()->builtins.reset(builtIns);
    void *srcHostPtr = alignedMalloc(512, 64);
    size_t hostPtrOffset = 2;

    // retrieve original builder
    auto &origBuilder = builtIns->getBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice());
    ASSERT_NE(nullptr, &origBuilder);

    // substitute original builder with mock builder
    auto oldBuilder = builtIns->setBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(*builtIns, &origBuilder)));
    EXPECT_EQ(&origBuilder, oldBuilder.get());

    // call enqueue on mock builder
    auto retVal = pCmdQ->enqueueSVMMemcpy(
        false,                                // cl_bool  blocking_copy
        dstSvmPtr,                            // void *dst_ptr
        ptrOffset(srcHostPtr, hostPtrOffset), // const void *src_ptr
        256,                                  // size_t size
        0,                                    // cl_uint num_events_in_wait_list
        nullptr,                              // cl_event *event_wait_list
        nullptr                               // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    // restore original builder and retrieve mock builder
    auto newBuilder = builtIns->setBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);

    // check if original builder is restored correctly
    auto &restoredBuilder = builtIns->getBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice());
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

    auto builtIns = new MockBuiltins();
    pCmdQ->getDevice().getExecutionEnvironment()->builtins.reset(builtIns);
    auto dstHostPtr = alignedMalloc(512, 64);
    size_t hostPtrOffset = 2;

    // retrieve original builder
    auto &origBuilder = builtIns->getBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice());
    ASSERT_NE(nullptr, &origBuilder);

    // substitute original builder with mock builder
    auto oldBuilder = builtIns->setBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(*builtIns, &origBuilder)));
    EXPECT_EQ(&origBuilder, oldBuilder.get());

    // call enqueue on mock builder
    auto retVal = pCmdQ->enqueueSVMMemcpy(
        false,                                // cl_bool  blocking_copy
        ptrOffset(dstHostPtr, hostPtrOffset), // void *dst_ptr
        srcSvmPtr,                            // const void *src_ptr
        256,                                  // size_t size
        0,                                    // cl_uint num_events_in_wait_list
        nullptr,                              // cl_event *event_wait_list
        nullptr                               // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    // restore original builder and retrieve mock builder
    auto newBuilder = builtIns->setBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);

    // check if original builder is restored correctly
    auto &restoredBuilder = builtIns->getBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice());
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

struct EnqueueSvmMemCopyHw : public ::testing::Test {

    void SetUp() override {

        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
        if (is32bit || !device->isFullRangeSvm()) {
            GTEST_SKIP();
        }

        context = std::make_unique<MockContext>(device.get());
        srcSvmPtr = context->getSVMAllocsManager()->createSVMAlloc(device->getRootDeviceIndex(), 256, {});
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
        nullptr                       // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueSvmMemCopyHwTest, givenEnqueueSVMMemCopyWhenUsingCopyBufferToBufferStatefulBuilderThenSuccessIsReturned) {
    auto cmdQ = std::make_unique<CommandQueueStateful<FamilyType>>(context.get(), device.get());

    auto retVal = cmdQ->enqueueSVMMemcpy(
        false,                          // cl_bool  blocking_copy
        dstHostPtr,                     // void *dst_ptr
        srcSvmPtr,                      // const void *src_ptr
        static_cast<size_t>(smallSize), // size_t size
        0,                              // cl_uint num_events_in_wait_list
        nullptr,                        // cl_event *event_wait_list
        nullptr                         // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}
