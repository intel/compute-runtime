/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_builtin_dispatch_info_builder.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

using namespace NEO;

struct BaseEnqueueSvmMemFillFixture : public ClDeviceFixture,
                                      public CommandQueueHwFixture {
    typedef CommandQueueHwFixture CommandQueueFixture;

    void setUp() {
        ClDeviceFixture::setUp();
        CommandQueueFixture::setUp(pClDevice, 0);
        SVMAllocsManager::SvmAllocationProperties svmProperties;
        svmProperties.coherent = true;
        svmPtr = context->getSVMAllocsManager()->createSVMAlloc(256, svmProperties, context->getRootDeviceIndices(), context->getDeviceBitfields());
        ASSERT_NE(nullptr, svmPtr);
        auto svmData = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
        ASSERT_NE(nullptr, svmData);
        svmAlloc = svmData->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
        ASSERT_NE(nullptr, svmAlloc);

        auto &compilerProductHelper = pDevice->getCompilerProductHelper();
        this->useHeapless = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
        this->useStateless = compilerProductHelper.isForceToStatelessRequired();
    }

    void tearDown() {
        if (svmPtr) {
            context->getSVMAllocsManager()->freeSVMAlloc(svmPtr);
        }
        CommandQueueFixture::tearDown();
        ClDeviceFixture::tearDown();
    }
    void *svmPtr = nullptr;
    GraphicsAllocation *svmAlloc = nullptr;
    const uint64_t pattern[16] = {0x0011223344556677, 0x8899AABBCCDDEEFF, 0xFFEEDDCCBBAA9988, 0x7766554433221100,
                                  0xFFEEDDCCBBAA9988, 0x7766554433221100, 0x0011223344556677, 0x8899AABBCCDDEEFF};
    size_t patternSize = 0x10;

    EBuiltInOps::Type getAdjustedFillBufferBuiltIn() {
        if (useHeapless) {
            return EBuiltInOps::fillBufferStatelessHeapless;
        } else if (useStateless) {
            return EBuiltInOps::fillBufferStateless;
        }

        return EBuiltInOps::fillBuffer;
    }

    bool useHeapless = false;
    bool useStateless = false;
};

using BaseEnqueueSvmMemFillTest = Test<BaseEnqueueSvmMemFillFixture>;

HWTEST_F(BaseEnqueueSvmMemFillTest, givenEnqueueSVMMemFillWhenUsingFillBufferBuilderThenUseGpuAddressForPatchingOfPatternSVMAllocation) {
    struct MockFillBufferBuilder : MockBuiltinDispatchInfoBuilder {
        MockFillBufferBuilder(BuiltIns &kernelLib, ClDevice &clDevice, BuiltinDispatchInfoBuilder *origBuilder, const void *pattern, size_t patternSize)
            : MockBuiltinDispatchInfoBuilder(kernelLib, clDevice, origBuilder),
              pattern(pattern), patternSize(patternSize) {
        }
        const void *pattern;
        size_t patternSize;
    };

    auto builtIn = getAdjustedFillBufferBuiltIn();

    auto builtIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()].get(), builtIns);

    auto &origBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        builtIn,
        pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &origBuilder);

    // note that we need to store the returned value, as it is an unique pointer storing original builder, which will be later invoked
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        builtIn,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockFillBufferBuilder(*builtIns, pCmdQ->getClDevice(), &origBuilder, pattern, patternSize)));

    size_t patternSize = 0x10u;
    auto patternAllocation = static_cast<MockGraphicsAllocation *>(context->getMemoryManager()->allocateGraphicsMemoryWithProperties({pCmdQ->getDevice().getRootDeviceIndex(), 2 * patternSize, AllocationType::fillPattern, pCmdQ->getDevice().getDeviceBitfield()}));

    // offset cpuPtr so cpuPtr != gpuAddress (for patching) in order to ensure that setArgSVM will be called using gpu address of the pattern allocation
    auto origCpuPtr = patternAllocation->cpuPtr;
    patternAllocation->cpuPtr = ptrOffset(patternAllocation->cpuPtr, patternSize);
    ASSERT_NE((uint64_t)patternAllocation->cpuPtr, patternAllocation->getGpuAddressToPatch());

    auto internalAllocStorage = pCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage();
    internalAllocStorage->storeAllocation(std::unique_ptr<GraphicsAllocation>(patternAllocation), REUSABLE_ALLOCATION);

    auto retVal = pCmdQ->enqueueSVMMemFill(
        svmPtr,
        pattern,
        patternSize,
        256,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // return mock builder
    auto &newBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        builtIn,
        pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &origBuilder);

    auto &mockBuilder = static_cast<MockFillBufferBuilder &>(newBuilder);
    auto kernel = mockBuilder.getMultiDispatchInfo()->begin()->getKernel();

    auto patternArgIndex = 2;
    const auto &patternArg = kernel->getKernelArguments().at(patternArgIndex);
    EXPECT_EQ(patternAllocation->getGpuAddressToPatch(), reinterpret_cast<uint64_t>(patternArg.value));

    patternAllocation->cpuPtr = origCpuPtr;
}

struct EnqueueSvmMemFillTest : public BaseEnqueueSvmMemFillFixture,
                               public ::testing::TestWithParam<size_t> {
    typedef CommandQueueHwFixture CommandQueueFixture;

    void SetUp() override {
        BaseEnqueueSvmMemFillFixture::setUp();
        patternSize = (size_t)GetParam();
        ASSERT_TRUE((0 < patternSize) && (patternSize <= 128));
    }

    void TearDown() override {
        BaseEnqueueSvmMemFillFixture::tearDown();
    }
};

HWTEST_P(EnqueueSvmMemFillTest, givenEnqueueSVMMemFillWhenUsingFillBufferBuilderThenItIsConfiguredWithBuitinOpParamsAndProducesDispatchInfo) {
    struct MockFillBufferBuilder : MockBuiltinDispatchInfoBuilder {
        MockFillBufferBuilder(BuiltIns &kernelLib, ClDevice &clDevice, BuiltinDispatchInfoBuilder *origBuilder, const void *pattern, size_t patternSize)
            : MockBuiltinDispatchInfoBuilder(kernelLib, clDevice, origBuilder),
              pattern(pattern), patternSize(patternSize) {
        }
        void validateInput(const BuiltinOpParams &conf) const override {
            auto patternAllocation = conf.srcMemObj->getMultiGraphicsAllocation().getDefaultGraphicsAllocation();
            EXPECT_EQ(MemoryConstants::pageSize, patternAllocation->getUnderlyingBufferSize());
            EXPECT_EQ(0, memcmp(pattern, patternAllocation->getUnderlyingBuffer(), patternSize));
        };
        const void *pattern;
        size_t patternSize;
    };

    auto builtIn = getAdjustedFillBufferBuiltIn();

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
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockFillBufferBuilder(*builtIns, pCmdQ->getClDevice(), &origBuilder, pattern, patternSize)));
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
    EXPECT_STREQ(EBuiltInOps::isStateless(builtIn) ? "FillBufferMiddleStateless" : "FillBufferMiddle", kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.c_str());
}

INSTANTIATE_TEST_SUITE_P(size_t,
                         EnqueueSvmMemFillTest,
                         ::testing::Values(1, 2, 4, 8, 16, 32, 64, 128));

struct EnqueueSvmMemFillHw : public ::testing::Test {

    void SetUp() override {

        device = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        if (is32bit || !device->isFullRangeSvm()) {
            GTEST_SKIP();
        }

        context = std::make_unique<MockContext>(device.get());
        svmPtr = context->getSVMAllocsManager()->createSVMAlloc(256, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
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

HWTEST_F(EnqueueSvmMemFillHwTest, givenEnqueueSVMMemFillWhenUsingCopyBufferToSystemBufferStatelessBuilderThenSuccessIsReturned) {
    auto cmdQ = std::make_unique<CommandQueueStateless<FamilyType>>(context.get(), device.get());
    auto svmData = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
    svmData->size = static_cast<size_t>(bigSize);
    svmData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex())->setAllocationType(AllocationType::svmZeroCopy);

    cmdQ->validateKernelSystemMemory = true;
    cmdQ->expectedKernelSystemMemory = true;

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

HWTEST2_F(EnqueueSvmMemFillHwTest, givenEnqueueSVMMemFillWhenUsingCopyBufferToLocalBufferStatefulBuilderThenSuccessIsReturned, IsStatefulBufferPreferredForProduct) {
    auto cmdQ = std::make_unique<CommandQueueStateful<FamilyType>>(context.get(), device.get());
    if (cmdQ->getHeaplessModeEnabled()) {
        GTEST_SKIP();
    }
    auto svmData = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
    svmData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex())->setAllocationType(AllocationType::svmGpu);

    cmdQ->validateKernelSystemMemory = true;
    cmdQ->expectedKernelSystemMemory = false;

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
