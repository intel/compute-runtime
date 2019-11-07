/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/unified_memory_manager.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "test.h"
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

        CommandQueueFixture::SetUp(pDevice, 0);
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
    void *srcHostPtr = alignedMalloc(256, 64);

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
        false,      // cl_bool  blocking_copy
        dstSvmPtr,  // void *dst_ptr
        srcHostPtr, // const void *src_ptr
        256,        // size_t size
        0,          // cl_uint num_events_in_wait_list
        nullptr,    // cl_event *event_wait_list
        nullptr     // cL_event *event
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
    EXPECT_EQ(srcHostPtr, params->srcPtr);
    EXPECT_EQ(dstSvmPtr, params->dstPtr);
    EXPECT_EQ(nullptr, params->srcMemObj);
    EXPECT_EQ(nullptr, params->dstMemObj);
    EXPECT_EQ(nullptr, params->srcSvmAlloc);
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
    alignedFree(srcHostPtr);
}

HWTEST_F(EnqueueSvmMemCopyTest, givenEnqueueSVMMemcpyWhenUsingCopyBufferToBufferBuilderAndDstHostPtrThenItConfiguredWithBuiltinOpsAndProducesDispatchInfo) {
    if (!pDevice->isFullRangeSvm()) {
        return;
    }

    auto builtIns = new MockBuiltins();
    pCmdQ->getDevice().getExecutionEnvironment()->builtins.reset(builtIns);
    auto dstHostPtr = alignedMalloc(256, 64);

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
        false,      // cl_bool  blocking_copy
        dstHostPtr, // void *dst_ptr
        srcSvmPtr,  // const void *src_ptr
        256,        // size_t size
        0,          // cl_uint num_events_in_wait_list
        nullptr,    // cl_event *event_wait_list
        nullptr     // cL_event *event
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
    EXPECT_EQ(dstHostPtr, params->dstPtr);
    EXPECT_EQ(nullptr, params->srcMemObj);
    EXPECT_EQ(nullptr, params->dstMemObj);
    EXPECT_EQ(srcSvmAlloc, params->srcSvmAlloc);
    EXPECT_EQ(nullptr, params->dstSvmAlloc);
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
    alignedFree(dstHostPtr);
}
