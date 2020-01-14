/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/unified_memory_manager.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "test.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_builtin_dispatch_info_builder.h"
#include "unit_tests/mocks/mock_builtins.h"

using namespace NEO;

struct EnqueueSvmMemFillTest : public DeviceFixture,
                               public CommandQueueHwFixture,
                               public ::testing::TestWithParam<size_t> {
    typedef CommandQueueHwFixture CommandQueueFixture;

    EnqueueSvmMemFillTest() {
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(pClDevice, 0);
        const HardwareInfo &hwInfo = pDevice->getHardwareInfo();
        if (!hwInfo.capabilityTable.ftrSvm) {
            GTEST_SKIP();
        }
        patternSize = (size_t)GetParam();
        ASSERT_TRUE((0 < patternSize) && (patternSize <= 128));
        SVMAllocsManager::SvmAllocationProperties svmProperties;
        svmProperties.coherent = true;
        svmPtr = context->getSVMAllocsManager()->createSVMAlloc(pDevice->getRootDeviceIndex(), 256, svmProperties);
        ASSERT_NE(nullptr, svmPtr);
        auto svmData = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
        ASSERT_NE(nullptr, svmData);
        svmAlloc = svmData->gpuAllocation;
        ASSERT_NE(nullptr, svmAlloc);
    }

    void TearDown() override {
        if (svmPtr) {
            context->getSVMAllocsManager()->freeSVMAlloc(svmPtr);
        }
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    const uint64_t pattern[16] = {0x0011223344556677, 0x8899AABBCCDDEEFF, 0xFFEEDDCCBBAA9988, 0x7766554433221100,
                                  0xFFEEDDCCBBAA9988, 0x7766554433221100, 0x0011223344556677, 0x8899AABBCCDDEEFF};
    size_t patternSize = 0;
    void *svmPtr = nullptr;
    GraphicsAllocation *svmAlloc = nullptr;
};

HWTEST_P(EnqueueSvmMemFillTest, givenEnqueueSVMMemFillWhenUsingFillBufferBuilderThenItIsConfiguredWithBuitinOpParamsAndProducesDispatchInfo) {
    struct MockFillBufferBuilder : MockBuiltinDispatchInfoBuilder {
        MockFillBufferBuilder(BuiltIns &kernelLib, BuiltinDispatchInfoBuilder *origBuilder, const void *pattern, size_t patternSize)
            : MockBuiltinDispatchInfoBuilder(kernelLib, origBuilder),
              pattern(pattern), patternSize(patternSize) {
        }
        void validateInput(const BuiltinOpParams &conf) const override {
            auto patternAllocation = conf.srcMemObj->getGraphicsAllocation();
            EXPECT_EQ(patternSize, patternAllocation->getUnderlyingBufferSize());
            EXPECT_EQ(0, memcmp(pattern, patternAllocation->getUnderlyingBuffer(), patternSize));
        };
        const void *pattern;
        size_t patternSize;
    };

    auto builtIns = new MockBuiltins();
    pCmdQ->getDevice().getExecutionEnvironment()->builtins.reset(builtIns);

    // retrieve original builder
    auto &origBuilder = builtIns->getBuiltinDispatchInfoBuilder(
        EBuiltInOps::FillBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice());
    ASSERT_NE(nullptr, &origBuilder);

    // substitute original builder with mock builder
    auto oldBuilder = builtIns->setBuiltinDispatchInfoBuilder(
        EBuiltInOps::FillBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockFillBufferBuilder(*builtIns, &origBuilder, pattern, patternSize)));
    EXPECT_EQ(&origBuilder, oldBuilder.get());

    // call enqueue on mock builder
    auto retVal = pCmdQ->enqueueSVMMemFill(
        svmPtr,      // void *svm_ptr
        pattern,     // const void *pattern
        patternSize, // size_t pattern_size
        256,         // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // cl_event *event_wait_list
        nullptr      // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    // restore original builder and retrieve mock builder
    auto newBuilder = builtIns->setBuiltinDispatchInfoBuilder(
        EBuiltInOps::FillBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);

    // check if original builder is restored correctly
    auto &restoredBuilder = builtIns->getBuiltinDispatchInfoBuilder(
        EBuiltInOps::FillBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice());
    EXPECT_EQ(&origBuilder, &restoredBuilder);

    // use mock builder to validate builder's input / output
    auto mockBuilder = static_cast<MockFillBufferBuilder *>(newBuilder.get());

    // validate builder's input - builtin ops
    auto params = mockBuilder->getBuiltinOpParams();
    EXPECT_EQ(nullptr, params->srcPtr);
    EXPECT_EQ(svmPtr, params->dstPtr);
    EXPECT_NE(nullptr, params->srcMemObj);
    EXPECT_EQ(nullptr, params->dstMemObj);
    EXPECT_EQ(nullptr, params->srcSvmAlloc);
    EXPECT_EQ(svmAlloc, params->dstSvmAlloc);
    EXPECT_EQ(Vec3<size_t>(0, 0, 0), params->srcOffset);
    EXPECT_EQ(Vec3<size_t>(0, 0, 0), params->dstOffset);
    EXPECT_EQ(Vec3<size_t>(256, 0, 0), params->size);

    // validate builder's output - multi dispatch info
    auto mdi = mockBuilder->getMultiDispatchInfo();
    EXPECT_EQ(1u, mdi->size());

    auto di = mdi->begin();
    size_t middleElSize = sizeof(uint32_t);
    EXPECT_EQ(Vec3<size_t>(256 / middleElSize, 1, 1), di->getGWS());

    auto kernel = di->getKernel();
    EXPECT_STREQ("FillBufferMiddle", kernel->getKernelInfo().name.c_str());
}

INSTANTIATE_TEST_CASE_P(size_t,
                        EnqueueSvmMemFillTest,
                        ::testing::Values(1, 2, 4, 8, 16, 32, 64, 128));

struct EnqueueSvmMemFillHw : public ::testing::Test {

    void SetUp() override {

        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
        if (is32bit || !device->isFullRangeSvm()) {
            GTEST_SKIP();
        }

        context = std::make_unique<MockContext>(device.get());
        svmPtr = context->getSVMAllocsManager()->createSVMAlloc(device->getRootDeviceIndex(), 256, {});
        ASSERT_NE(nullptr, svmPtr);
    }

    void TearDown() override {
        if (is32bit || !device->isFullRangeSvm()) {
            return;
        }
        context->getSVMAllocsManager()->freeSVMAlloc(svmPtr);
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    uint64_t bigSize = 5ull * MemoryConstants::gigaByte;
    uint64_t smallSize = 4ull * MemoryConstants::gigaByte - 1;
    void *svmPtr = nullptr;
    const uint64_t pattern[4] = {0x0011223344556677,
                                 0x8899AABBCCDDEEFF,
                                 0xFFEEDDCCBBAA9988,
                                 0x7766554433221100};
    size_t patternSize = 0;
};

using EnqueueSvmMemFillHwTest = EnqueueSvmMemFillHw;

HWTEST_F(EnqueueSvmMemFillHwTest, givenEnqueueSVMMemFillWhenUsingCopyBufferToBufferStatelessBuilderThenSuccessIsReturned) {
    auto cmdQ = std::make_unique<CommandQueueStateless<FamilyType>>(context.get(), device.get());
    auto svmData = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
    svmData->size = static_cast<size_t>(bigSize);

    auto retVal = cmdQ->enqueueSVMMemFill(
        svmPtr,                       // void *svm_ptr
        pattern,                      // const void *pattern
        patternSize,                  // size_t pattern_size
        static_cast<size_t>(bigSize), // size_t size
        0,                            // cl_uint num_events_in_wait_list
        nullptr,                      // cl_event *event_wait_list
        nullptr                       // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueSvmMemFillHwTest, givenEnqueueSVMMemFillWhenUsingCopyBufferToBufferStatefulBuilderThenSuccessIsReturned) {
    auto cmdQ = std::make_unique<CommandQueueStateful<FamilyType>>(context.get(), device.get());
    auto retVal = cmdQ->enqueueSVMMemFill(
        svmPtr,                         // void *svm_ptr
        pattern,                        // const void *pattern
        patternSize,                    // size_t pattern_size
        static_cast<size_t>(smallSize), // size_t size
        0,                              // cl_uint num_events_in_wait_list
        nullptr,                        // cl_event *event_wait_list
        nullptr                         // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}
