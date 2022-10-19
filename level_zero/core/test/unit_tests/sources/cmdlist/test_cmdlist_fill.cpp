/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include <limits>

namespace L0 {
namespace ult {

class AppendFillFixture : public DeviceFixture {
  public:
    class MockDriverFillHandle : public L0::DriverHandleImp {
      public:
        bool findAllocationDataForRange(const void *buffer,
                                        size_t size,
                                        NEO::SvmAllocationData **allocData) override {
            mockAllocation.reset(new NEO::MockGraphicsAllocation(const_cast<void *>(buffer), size));
            data.gpuAllocations.addAllocation(mockAllocation.get());
            if (allocData) {
                *allocData = &data;
            }
            return true;
        }
        const uint32_t rootDeviceIndex = 0u;
        std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
        NEO::SvmAllocationData data{rootDeviceIndex};
    };

    template <GFXCORE_FAMILY gfxCoreFamily>
    class MockCommandList : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
      public:
        MockCommandList() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}

        ze_result_t appendLaunchKernelWithParams(Kernel *kernel,
                                                 const ze_group_count_t *pThreadGroupDimensions,
                                                 Event *event,
                                                 const CmdListKernelLaunchParams &launchParams) override {
            if (numberOfCallsToAppendLaunchKernelWithParams == thresholdOfCallsToAppendLaunchKernelWithParamsToFail) {
                return ZE_RESULT_ERROR_UNKNOWN;
            }
            if (numberOfCallsToAppendLaunchKernelWithParams < 3) {
                threadGroupDimensions[numberOfCallsToAppendLaunchKernelWithParams] = *pThreadGroupDimensions;
                xGroupSizes[numberOfCallsToAppendLaunchKernelWithParams] = kernel->getGroupSize()[0];
            }
            numberOfCallsToAppendLaunchKernelWithParams++;
            return CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(kernel,
                                                                                      pThreadGroupDimensions,
                                                                                      event,
                                                                                      launchParams);
        }
        ze_group_count_t threadGroupDimensions[3];
        uint32_t xGroupSizes[3];
        uint32_t thresholdOfCallsToAppendLaunchKernelWithParamsToFail = std::numeric_limits<uint32_t>::max();
        uint32_t numberOfCallsToAppendLaunchKernelWithParams = 0;
    };

    void setUp() {
        dstPtr = new uint8_t[allocSize];
        immediateDstPtr = new uint8_t[allocSize];

        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<MockDriverFillHandle>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
        neoDevice->deviceInfo.maxWorkGroupSize = 256;
    }

    void tearDown() {
        delete[] immediateDstPtr;
        delete[] dstPtr;
    }

    DebugManagerStateRestore restorer;

    std::unique_ptr<Mock<MockDriverFillHandle>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    static constexpr size_t allocSize = 70;
    static constexpr size_t patternSize = 8;
    uint8_t *dstPtr = nullptr;
    uint8_t pattern[patternSize] = {1, 2, 3, 4};

    static constexpr size_t immediateAllocSize = 106;
    uint8_t immediatePattern = 4;
    uint8_t *immediateDstPtr = nullptr;
};

struct MultiTileAppendFillFixture : public AppendFillFixture {
    void setUp() {
        DebugManager.flags.CreateMultipleSubDevices.set(2);
        DebugManager.flags.EnableImplicitScaling.set(1);
        AppendFillFixture::setUp();
    }
};

using AppendFillTest = Test<AppendFillFixture>;

using MultiTileAppendFillTest = Test<MultiTileAppendFillFixture>;

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWithImmediateValueThenSuccessIsReturned, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    auto result = commandList->appendMemoryFill(immediateDstPtr, &immediatePattern,
                                                sizeof(immediatePattern),
                                                immediateAllocSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillThenSuccessIsReturned, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    auto result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWithAppendLaunchKernelFailureThenSuccessIsNotReturned, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    commandList->thresholdOfCallsToAppendLaunchKernelWithParamsToFail = 0;

    auto result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(AppendFillTest,
          givenTwoCallsToAppendMemoryFillWithSamePatternThenAllocationIsCreatedForEachCall, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    ze_result_t result = commandList->appendMemoryFill(dstPtr, pattern, 4, allocSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t patternAllocationsVectorSize = commandList->patternAllocations.size();
    EXPECT_EQ(patternAllocationsVectorSize, 1u);

    uint8_t *newDstPtr = new uint8_t[allocSize];
    result = commandList->appendMemoryFill(newDstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t newPatternAllocationsVectorSize = commandList->patternAllocations.size();

    EXPECT_GT(newPatternAllocationsVectorSize, patternAllocationsVectorSize);

    delete[] newDstPtr;
}

HWTEST2_F(AppendFillTest,
          givenTwoCallsToAppendMemoryFillWithDifferentPatternsThenAllocationIsCreatedForEachPattern, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    ze_result_t result = commandList->appendMemoryFill(dstPtr, pattern, 4, allocSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t patternAllocationsVectorSize = commandList->patternAllocations.size();
    EXPECT_EQ(patternAllocationsVectorSize, 1u);

    uint8_t newPattern[patternSize] = {1, 2, 3, 4};
    result = commandList->appendMemoryFill(dstPtr, newPattern, patternSize, allocSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t newPatternAllocationsVectorSize = commandList->patternAllocations.size();

    EXPECT_EQ(patternAllocationsVectorSize + 1u, newPatternAllocationsVectorSize);
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWhenPatternSizeIsOneThenDispatchOneKernel, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    int pattern = 0;
    const size_t size = 1024 * 1024;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size, commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWithUnalignedSizeWhenPatternSizeIsOneThenDispatchTwoKernels, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    int pattern = 0;
    const size_t size = 1025;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16;
    filledSize += commandList->xGroupSizes[1] * commandList->threadGroupDimensions[1].groupCountX;
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size, filledSize);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWithSizeBelow16WhenPatternSizeIsOneThenDispatchTwoKernels, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    int pattern = 0;
    const size_t size = 4;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16;
    filledSize += commandList->xGroupSizes[1] * commandList->threadGroupDimensions[1].groupCountX;
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size, filledSize);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWithSizeBelowMaxWorkgroupSizeWhenPatternSizeIsOneThenDispatchOneKernel, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    int pattern = 0;
    const size_t size = neoDevice->getDeviceInfo().maxWorkGroupSize / 2;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16;
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size, filledSize);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWhenPatternSizeIsOneThenGroupCountIsCorrect, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    int pattern = 0;
    const size_t size = 1024 * 1024;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr, &pattern, 1, size, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto groupSize = device->getDeviceInfo().maxWorkGroupSize;
    auto dataTypeSize = sizeof(uint32_t) * 4;
    auto expectedGroupCount = size / (dataTypeSize * groupSize);
    EXPECT_EQ(expectedGroupCount, commandList->threadGroupDimensions[0].groupCountX);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWhenPtrWithOffsetAndPatternSizeIsOneThenThreeKernelsDispatched, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    int pattern = 0;
    uint32_t offset = 1;
    const size_t size = 1024;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr + offset, &pattern, 1, size - offset, nullptr, 0, nullptr);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX;
    filledSize += commandList->xGroupSizes[1] * commandList->threadGroupDimensions[1].groupCountX * 16;
    filledSize += commandList->xGroupSizes[2] * commandList->threadGroupDimensions[2].groupCountX;
    EXPECT_EQ(sizeof(uint32_t) - offset, commandList->xGroupSizes[0]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(3u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size - offset, filledSize);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWhenPtrWithOffsetAndSmallSizeAndPatternSizeIsOneThenTwoKernelsDispatched, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    int pattern = 0;
    uint32_t offset = 1;
    const size_t size = 2;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr + offset, &pattern, 1, size - offset, nullptr, 0, nullptr);
    size_t filledSize = commandList->xGroupSizes[0] * commandList->threadGroupDimensions[0].groupCountX * 16;
    filledSize += commandList->xGroupSizes[1] * commandList->threadGroupDimensions[1].groupCountX;
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, commandList->numberOfCallsToAppendLaunchKernelWithParams);
    EXPECT_EQ(size - offset, filledSize);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenAppendMemoryFillWhenPtrWithOffsetAndFailAppendUnalignedFillKernelThenReturnError, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->thresholdOfCallsToAppendLaunchKernelWithParamsToFail = 0;
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    int pattern = 0;
    uint32_t offset = 1;
    const size_t size = 1024;
    uint8_t *ptr = new uint8_t[size];
    ze_result_t result = commandList->appendMemoryFill(ptr + offset, &pattern, 1, size - offset, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);
    delete[] ptr;
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWithSizeNotMultipleOfPatternSizeThenSuccessIsReturned, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    size_t nonMultipleSize = allocSize + 1;
    uint8_t *nonMultipleDstPtr = new uint8_t[nonMultipleSize];
    auto result = commandList->appendMemoryFill(nonMultipleDstPtr, pattern, 4, nonMultipleSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    delete[] nonMultipleDstPtr;
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWithSizeNotMultipleOfPatternSizeAndAppendLaunchKernelFailureOnRemainderThenSuccessIsNotReturned, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    commandList->thresholdOfCallsToAppendLaunchKernelWithParamsToFail = 1;

    size_t nonMultipleSize = allocSize + 1;
    uint8_t *nonMultipleDstPtr = new uint8_t[nonMultipleSize];
    auto result = commandList->appendMemoryFill(nonMultipleDstPtr, pattern, 4, nonMultipleSize, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);

    delete[] nonMultipleDstPtr;
}

using IsBetweenGen9AndGen12lp = IsWithinGfxCore<IGFX_GEN9_CORE, IGFX_GEN12LP_CORE>;

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWithImmediateValueWhenTimestampEventUsesRegistersThenSinglePacketUsesRegisterProfiling, IsBetweenGen9AndGen12lp) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    uint64_t globalStartAddress = event->getGpuAddress(device) + event->getGlobalStartOffset();
    uint64_t contextStartAddress = event->getGpuAddress(device) + event->getContextStartOffset();
    uint64_t globalEndAddress = event->getGpuAddress(device) + event->getGlobalEndOffset();
    uint64_t contextEndAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    result = commandList->appendMemoryFill(immediateDstPtr, &immediatePattern,
                                           sizeof(immediatePattern),
                                           immediateAllocSize, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, event->getPacketsInUse());
    EXPECT_EQ(1u, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList->commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    auto begin = cmdList.begin();
    ASSERT_EQ(2u, itorWalkers.size());
    auto secondWalker = itorWalkers[1];

    validateTimestampRegisters<FamilyType>(cmdList,
                                           begin,
                                           REG_GLOBAL_TIMESTAMP_LDW, globalStartAddress,
                                           GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, contextStartAddress,
                                           false);

    validateTimestampRegisters<FamilyType>(cmdList,
                                           secondWalker,
                                           REG_GLOBAL_TIMESTAMP_LDW, globalEndAddress,
                                           GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, contextEndAddress,
                                           false);
}

HWTEST2_F(AppendFillTest,
          givenCallToAppendMemoryFillWhenTimestampEventUsesRegistersThenSinglePacketUsesRegisterProfiling, IsBetweenGen9AndGen12lp) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    uint64_t globalStartAddress = event->getGpuAddress(device) + event->getGlobalStartOffset();
    uint64_t contextStartAddress = event->getGpuAddress(device) + event->getContextStartOffset();
    uint64_t globalEndAddress = event->getGpuAddress(device) + event->getGlobalEndOffset();
    uint64_t contextEndAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, event->getPacketsInUse());
    EXPECT_EQ(1u, event->getKernelCount());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList->commandContainer.getCommandStream()->getUsed()));

    auto itorWalkers = findAll<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    auto begin = cmdList.begin();
    ASSERT_EQ(2u, itorWalkers.size());
    auto secondWalker = itorWalkers[1];

    validateTimestampRegisters<FamilyType>(cmdList,
                                           begin,
                                           REG_GLOBAL_TIMESTAMP_LDW, globalStartAddress,
                                           GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, contextStartAddress,
                                           false);

    validateTimestampRegisters<FamilyType>(cmdList,
                                           secondWalker,
                                           REG_GLOBAL_TIMESTAMP_LDW, globalEndAddress,
                                           GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, contextEndAddress,
                                           false);
}

template <int32_t usePipeControlMultiPacketEventSync>
struct AppendFillMultiPacketEventFixture : public AppendFillFixture {
    void setUp() {
        DebugManager.flags.UsePipeControlMultiKernelEventSync.set(usePipeControlMultiPacketEventSync);
        AppendFillFixture::setUp();
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendMemoryFillManyImmediateKernels() {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
        using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
        using OPERATION = typename POSTSYNC_DATA::OPERATION;

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;

        ze_result_t result = ZE_RESULT_SUCCESS;
        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

        uint64_t firstKernelEventAddress = postSyncAddressZero ? 0 : event->getGpuAddress(device);
        uint64_t secondKernelEventAddress = postSyncAddressZero ? 0 : event->getGpuAddress(device) + event->getSinglePacketSize();

        auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
        commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
        auto &commandContainer = commandList->commandContainer;

        size_t usedBefore = commandContainer.getCommandStream()->getUsed();
        result = commandList->appendMemoryFill(immediateDstPtr, &immediatePattern,
                                               sizeof(immediatePattern),
                                               immediateAllocSize, event->toHandle(), 0, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        size_t usedAfter = commandContainer.getCommandStream()->getUsed();

        EXPECT_EQ(expectedPacketsInUse, event->getPacketsInUse());
        EXPECT_EQ(expectedKernelCount, event->getKernelCount());

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
            usedAfter - usedBefore));

        auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(2u, itorWalkers.size());
        auto firstWalker = itorWalkers[0];
        auto secondWalker = itorWalkers[1];

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
        EXPECT_EQ(static_cast<OPERATION>(expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
        EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

        walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
        EXPECT_EQ(static_cast<OPERATION>(expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
        EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendMemoryFillManyKernels() {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
        using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
        using OPERATION = typename POSTSYNC_DATA::OPERATION;

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;

        ze_result_t result = ZE_RESULT_SUCCESS;
        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

        uint64_t firstKernelEventAddress = postSyncAddressZero ? 0 : event->getGpuAddress(device);
        uint64_t secondKernelEventAddress = postSyncAddressZero ? 0 : event->getGpuAddress(device) + event->getSinglePacketSize();

        auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
        commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
        auto &commandContainer = commandList->commandContainer;

        size_t usedBefore = commandContainer.getCommandStream()->getUsed();
        result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, event->toHandle(), 0, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        size_t usedAfter = commandContainer.getCommandStream()->getUsed();

        EXPECT_EQ(expectedPacketsInUse, event->getPacketsInUse());
        EXPECT_EQ(expectedKernelCount, event->getKernelCount());

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
            usedAfter - usedBefore));

        auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(2u, itorWalkers.size());
        auto firstWalker = itorWalkers[0];
        auto secondWalker = itorWalkers[1];

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
        EXPECT_EQ(static_cast<OPERATION>(expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
        EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

        walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
        EXPECT_EQ(static_cast<OPERATION>(expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
        EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendMemoryFillSingleKernel() {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
        using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
        using OPERATION = typename POSTSYNC_DATA::OPERATION;

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;

        ze_result_t result = ZE_RESULT_SUCCESS;
        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

        auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
        commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

        int pattern = 0;
        const size_t size = 1024;
        uint8_t array[size] = {};

        auto &commandContainer = commandList->commandContainer;
        size_t usedBefore = commandContainer.getCommandStream()->getUsed();
        result = commandList->appendMemoryFill(array, &pattern, 1, size, event->toHandle(), 0, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        size_t usedAfter = commandContainer.getCommandStream()->getUsed();

        EXPECT_EQ(expectedPacketsInUse, event->getPacketsInUse());
        EXPECT_EQ(expectedKernelCount, event->getKernelCount());

        uint64_t firstKernelEventAddress = postSyncAddressZero ? 0 : event->getGpuAddress(device);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
            usedAfter - usedBefore));

        auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, itorWalkers.size());
        auto firstWalker = itorWalkers[0];

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
        EXPECT_EQ(static_cast<OPERATION>(expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
        EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendMemoryFillSingleKernelAndL3Flush() {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
        using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
        using OPERATION = typename POSTSYNC_DATA::OPERATION;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

        ze_result_t result = ZE_RESULT_SUCCESS;
        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

        auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
        commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

        int pattern = 0;
        const size_t size = 1024;
        uint8_t array[size] = {};

        auto &commandContainer = commandList->commandContainer;
        size_t usedBefore = commandContainer.getCommandStream()->getUsed();
        result = commandList->appendMemoryFill(array, &pattern, 1, size, event->toHandle(), 0, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        size_t usedAfter = commandContainer.getCommandStream()->getUsed();

        EXPECT_EQ(expectedPacketsInUse, event->getPacketsInUse());
        EXPECT_EQ(expectedKernelCount, event->getKernelCount());

        uint64_t firstKernelEventAddress = postSyncAddressZero ? 0 : event->getGpuAddress(device);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
            usedAfter - usedBefore));

        auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, itorWalkers.size());
        auto firstWalker = itorWalkers[0];

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
        EXPECT_EQ(static_cast<OPERATION>(expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
        EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

        uint64_t l3FlushPostSyncAddress = firstKernelEventAddress + event->getSinglePacketSize();
        if (event->isUsingContextEndOffset()) {
            l3FlushPostSyncAddress += event->getContextEndOffset();
        }

        auto itorPipeControls = findAll<PIPE_CONTROL *>(firstWalker, cmdList.end());

        uint32_t postSyncPipeControls = 0;
        uint32_t dcFlushFound = 0;
        for (auto it : itorPipeControls) {
            auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
            if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                postSyncPipeControls++;
                EXPECT_EQ(l3FlushPostSyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
                EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
            }
            if (cmd->getDcFlushEnable()) {
                dcFlushFound++;
            }
        }
        EXPECT_EQ(expectedPostSyncPipeControls, postSyncPipeControls);
        EXPECT_EQ(1u, dcFlushFound);
    }

    uint32_t expectedPacketsInUse = 0;
    uint32_t expectedKernelCount = 0;
    uint32_t expectedWalkerPostSyncOp = 0;
    uint32_t expectedPostSyncPipeControls = 0;
    bool postSyncAddressZero = false;
};

using AppendFillMultiPacketEventTest = Test<AppendFillMultiPacketEventFixture<0>>;
using AppendFillSinglePacketEventTest = Test<AppendFillMultiPacketEventFixture<1>>;

HWTEST2_F(AppendFillMultiPacketEventTest,
          givenCallToAppendMemoryFillWithImmediateValueWhenTimestampEventUsesComputeWalkerPostSyncThenSeparateKernelsUsesPostSyncProfiling,
          IsAtLeastXeHpCore) {
    expectedPacketsInUse = 2;
    expectedKernelCount = 2;
    expectedWalkerPostSyncOp = 3;
    postSyncAddressZero = false;

    testAppendMemoryFillManyImmediateKernels<gfxCoreFamily>();
}

HWTEST2_F(AppendFillMultiPacketEventTest,
          givenCallToAppendMemoryFillWhenTimestampEventUsesComputeWalkerPostSyncThenSeparateKernelsUsesWalkerPostSyncProfiling,
          IsAtLeastXeHpCore) {
    expectedPacketsInUse = 2;
    expectedKernelCount = 2;
    expectedWalkerPostSyncOp = 3;
    postSyncAddressZero = false;

    testAppendMemoryFillManyKernels<gfxCoreFamily>();
}

HWTEST2_F(AppendFillMultiPacketEventTest,
          givenAppendMemoryFillUsingSinglePacketEventWhenPatternDispatchOneKernelThenUseComputeWalkerPostSync,
          IsAtLeastXeHpCore) {
    expectedPacketsInUse = 1;
    expectedKernelCount = 1;
    expectedWalkerPostSyncOp = 3;
    postSyncAddressZero = false;

    testAppendMemoryFillSingleKernel<gfxCoreFamily>();
}

HWTEST2_F(AppendFillMultiPacketEventTest,
          givenAppendMemoryFillUsingSinglePacketEventWhenPatternDispatchOneKernelThenUseComputeWalkerPostSyncAndL3PostSync,
          IsXeHpOrXeHpgCore) {
    expectedPacketsInUse = 2;
    expectedKernelCount = 1;
    expectedWalkerPostSyncOp = 3;
    expectedPostSyncPipeControls = 1;
    postSyncAddressZero = false;

    testAppendMemoryFillSingleKernelAndL3Flush<gfxCoreFamily>();
}

HWTEST2_F(AppendFillSinglePacketEventTest,
          givenCallToAppendMemoryFillWithImmediateValueWhenTimestampEventUsesRegisterPostSyncThenSeparateKernelsNotUsesWalkerPostSyncProfiling,
          IsAtLeastXeHpCore) {
    expectedPacketsInUse = 1;
    expectedKernelCount = 1;
    expectedWalkerPostSyncOp = 0;
    postSyncAddressZero = true;

    testAppendMemoryFillManyImmediateKernels<gfxCoreFamily>();
}

HWTEST2_F(AppendFillSinglePacketEventTest,
          givenCallToAppendMemoryFillWhenTimestampEventUsesRegisterPostSyncThenSeparateKernelsNotUsesWalkerPostSyncProfiling,
          IsAtLeastXeHpCore) {
    expectedPacketsInUse = 1;
    expectedKernelCount = 1;
    expectedWalkerPostSyncOp = 0;
    postSyncAddressZero = true;

    testAppendMemoryFillManyKernels<gfxCoreFamily>();
}

HWTEST2_F(AppendFillSinglePacketEventTest,
          givenAppendMemoryFillUsingSinglePacketEventWhenPatternDispatchOneKernelThenUseComputeWalkerPostSync,
          IsAtLeastXeHpCore) {
    expectedPacketsInUse = 1;
    expectedKernelCount = 1;
    expectedWalkerPostSyncOp = 3;
    postSyncAddressZero = false;

    testAppendMemoryFillSingleKernel<gfxCoreFamily>();
}

HWTEST2_F(AppendFillSinglePacketEventTest,
          givenAppendMemoryFillUsingSinglePacketEventWhenPatternDispatchOneKernelThenUseComputeWalkerPostSyncAndL3PostSync,
          IsXeHpOrXeHpgCore) {
    expectedPacketsInUse = 2;
    expectedKernelCount = 1;
    expectedWalkerPostSyncOp = 3;
    expectedPostSyncPipeControls = 1;
    postSyncAddressZero = false;

    testAppendMemoryFillSingleKernelAndL3Flush<gfxCoreFamily>();
}

template <int32_t usePipeControlMultiPacketEventSync>
struct MultiTileAppendFillMultiPacketEventFixture : public MultiTileAppendFillFixture {
    void setUp() {
        DebugManager.flags.UsePipeControlMultiKernelEventSync.set(usePipeControlMultiPacketEventSync);
        MultiTileAppendFillFixture::setUp();
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendMemoryFillManyKernels(ze_event_pool_flags_t eventPoolFlags) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
        using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
        using OPERATION = typename POSTSYNC_DATA::OPERATION;

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = eventPoolFlags;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

        ze_result_t result = ZE_RESULT_SUCCESS;
        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

        uint64_t firstKernelEventAddress = postSyncAddressZero ? 0 : event->getGpuAddress(device);
        uint64_t secondKernelEventAddress = postSyncAddressZero ? 0 : event->getGpuAddress(device) + 2 * event->getSinglePacketSize();

        auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
        commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
        EXPECT_EQ(2u, commandList->partitionCount);
        auto &commandContainer = commandList->commandContainer;

        size_t usedBefore = commandContainer.getCommandStream()->getUsed();
        result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, event->toHandle(), 0, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        size_t usedAfter = commandContainer.getCommandStream()->getUsed();

        uint32_t expectedDcFlush = 0;

        if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo())) {
            // 1st dc flush after cross-tile sync, 2nd dc flush for signal scope event
            expectedDcFlush = 2;
        }

        EXPECT_EQ(expectedPacketsInUse, event->getPacketsInUse());
        EXPECT_EQ(expectedKernelCount, event->getKernelCount());

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
            usedAfter - usedBefore));

        auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(2u, itorWalkers.size());
        auto firstWalker = itorWalkers[0];
        auto secondWalker = itorWalkers[1];

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
        EXPECT_EQ(static_cast<OPERATION>(expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
        EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

        walkerCmd = genCmdCast<COMPUTE_WALKER *>(*secondWalker);
        EXPECT_EQ(static_cast<OPERATION>(expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
        EXPECT_EQ(secondKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

        auto itorPipeControls = findAll<PIPE_CONTROL *>(secondWalker, cmdList.end());

        uint32_t postSyncPipeControls = 0;
        uint32_t dcFlushFound = 0;

        for (auto it : itorPipeControls) {
            auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
            if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
                postSyncPipeControls++;
            }
            if (cmd->getDcFlushEnable()) {
                dcFlushFound++;
            }
        }

        EXPECT_EQ(expectedPostSyncPipeControl, postSyncPipeControls);
        EXPECT_EQ(expectedDcFlush, dcFlushFound);
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendMemoryFillSingleKernelAndL3Flush(ze_event_pool_flags_t eventPoolFlags) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
        using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
        using OPERATION = typename POSTSYNC_DATA::OPERATION;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = eventPoolFlags;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

        ze_result_t result = ZE_RESULT_SUCCESS;
        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

        auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
        commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

        int pattern = 0;
        const size_t size = 1024;
        uint8_t array[size] = {};

        auto &commandContainer = commandList->commandContainer;
        size_t usedBefore = commandContainer.getCommandStream()->getUsed();
        result = commandList->appendMemoryFill(array, &pattern, 1, size, event->toHandle(), 0, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        size_t usedAfter = commandContainer.getCommandStream()->getUsed();

        EXPECT_EQ(expectedPacketsInUse, event->getPacketsInUse());
        EXPECT_EQ(expectedKernelCount, event->getKernelCount());

        uint64_t firstKernelEventAddress = postSyncAddressZero ? 0 : event->getGpuAddress(device);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
            usedAfter - usedBefore));

        auto itorWalkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, itorWalkers.size());
        auto firstWalker = itorWalkers[0];

        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*firstWalker);
        EXPECT_EQ(static_cast<OPERATION>(expectedWalkerPostSyncOp), walkerCmd->getPostSync().getOperation());
        EXPECT_EQ(firstKernelEventAddress, walkerCmd->getPostSync().getDestinationAddress());

        uint64_t l3FlushPostSyncAddress = firstKernelEventAddress + 2 * event->getSinglePacketSize();
        if (event->isUsingContextEndOffset()) {
            l3FlushPostSyncAddress += event->getContextEndOffset();
        }

        auto itorPipeControls = findAll<PIPE_CONTROL *>(firstWalker, cmdList.end());

        uint32_t postSyncPipeControls = 0;
        uint32_t dcFlushFound = 0;
        for (auto it : itorPipeControls) {
            auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
            if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                postSyncPipeControls++;
                EXPECT_EQ(l3FlushPostSyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
                EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
            }
            if (cmd->getDcFlushEnable()) {
                dcFlushFound++;
            }
        }

        constexpr uint32_t expectedDcFlush = 2; // dc flush for last cross-tile sync and separately for signal scope event after last kernel split
        EXPECT_EQ(expectedPostSyncPipeControl, postSyncPipeControls);
        EXPECT_EQ(expectedDcFlush, dcFlushFound);
    }

    uint32_t expectedPacketsInUse = 0;
    uint32_t expectedKernelCount = 0;
    uint32_t expectedWalkerPostSyncOp = 0;
    uint32_t expectedPostSyncPipeControl = 0;
    bool postSyncAddressZero = false;
};

using MultiTileAppendFillEventMultiPacketTest = Test<MultiTileAppendFillMultiPacketEventFixture<0>>;
using MultiTileAppendFillEventSinglePacketTest = Test<MultiTileAppendFillMultiPacketEventFixture<1>>;

HWTEST2_F(MultiTileAppendFillEventMultiPacketTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenSignalScopeTimestampEventUsesComputeWalkerPostSyncThenSeparateKernelsUsesWalkerPostSyncProfilingAndSingleDcFlushWithImmediatePostSync, IsAtLeastXeHpCore) {
    // two kernels and each kernel uses two packets (for two tiles), in total 4
    expectedPacketsInUse = 4;
    expectedKernelCount = 2;
    expectedWalkerPostSyncOp = 3;
    expectedPostSyncPipeControl = 0;
    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo())) {
        // last kernel uses 4 packets, in addition to kernel two packets, use 2 packets to two tile cache flush
        expectedPacketsInUse = 6;
        // cache flush with event signal
        expectedPostSyncPipeControl = 1;
    }
    postSyncAddressZero = false;
    testAppendMemoryFillManyKernels<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileAppendFillEventMultiPacketTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenSignalScopeImmediateEventUsesComputeWalkerPostSyncThenSeparateKernelsUsesWalkerPostSyncAndSingleDcFlushWithPostSync, IsAtLeastXeHpCore) {
    // two kernels and each kernel uses two packets (for two tiles), in total 4
    expectedPacketsInUse = 4;
    expectedKernelCount = 2;
    expectedWalkerPostSyncOp = 3;
    expectedPostSyncPipeControl = 0;
    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo())) {
        // last kernel uses 4 packets, in addition to kernel two packets, use 2 packets to two tile cache flush
        expectedPacketsInUse = 6;
        // cache flush with event signal
        expectedPostSyncPipeControl = 1;
    }
    postSyncAddressZero = false;
    testAppendMemoryFillManyKernels<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileAppendFillEventMultiPacketTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenSignalScopeTimestampEventUsesComputeWalkerPostSyncThenSingleKernelsUsesWalkerPostSyncProfilingAndSingleDcFlushWithImmediatePostSync, IsXeHpOrXeHpgCore) {
    // kernel uses 4 packets, in addition to kernel two packets, use 2 packets to two tile cache flush
    expectedPacketsInUse = 4;
    expectedKernelCount = 1;
    expectedWalkerPostSyncOp = 3;
    // cache flush with event signal
    expectedPostSyncPipeControl = 1;
    postSyncAddressZero = false;

    testAppendMemoryFillSingleKernelAndL3Flush<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileAppendFillEventMultiPacketTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenSignalScopeImmediateEventUsesComputeWalkerPostSyncThenSingleKernelUsesWalkerPostSyncAndSingleDcFlushWithPostSync, IsXeHpOrXeHpgCore) {
    // kernel uses 4 packets, in addition to kernel two packets, use 2 packets to two tile cache flush
    expectedPacketsInUse = 4;
    expectedKernelCount = 1;
    expectedWalkerPostSyncOp = 3;
    // cache flush with event signal
    expectedPostSyncPipeControl = 1;

    testAppendMemoryFillSingleKernelAndL3Flush<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileAppendFillEventSinglePacketTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenSignalScopeTimestampEventUsesRegisterPostSyncThenSeparateKernelsNotUsesWalkerPostSyncProfilingAndDcFlushWithNoPostSync, IsAtLeastXeHpCore) {
    expectedPacketsInUse = 2;
    expectedKernelCount = 1;
    expectedWalkerPostSyncOp = 0;
    expectedPostSyncPipeControl = 0;
    postSyncAddressZero = true;
    testAppendMemoryFillManyKernels<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileAppendFillEventSinglePacketTest,
          givenMultiTileCmdListCallToAppendMemoryFillWhenSignalScopeImmediateEventUsesPipeControlPostSyncThenSeparateKernelsNotUsesWalkerPostSyncProfilingAndDcFlushWithImmediatePostSync, IsAtLeastXeHpCore) {
    expectedPacketsInUse = 2;
    expectedKernelCount = 1;
    expectedPacketsInUse = 2;
    expectedWalkerPostSyncOp = 0;
    expectedPostSyncPipeControl = 1;
    postSyncAddressZero = true;
    testAppendMemoryFillManyKernels<gfxCoreFamily>(0);
}

} // namespace ult
} // namespace L0
