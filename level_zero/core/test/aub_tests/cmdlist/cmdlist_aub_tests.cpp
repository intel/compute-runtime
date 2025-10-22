/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/transfer_direction.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/bcs_split.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/aub_tests/fixtures/multicontext_l0_aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"
#include "level_zero/driver_experimental/zex_api.h"

extern std::optional<uint32_t> blitterMaskOverride;

namespace L0 {
namespace ult {

using AUBAppendKernelIndirectL0 = Test<AUBFixtureL0>;

TEST_F(AUBAppendKernelIndirectL0, whenAppendKernelIndirectThenGlobalWorkSizeIsProperlyProgrammed) {
    const uint32_t groupSize[] = {1, 2, 3};
    const uint32_t groupCount[] = {4, 3, 1};
    const uint32_t expectedGlobalWorkSize[] = {groupSize[0] * groupCount[0],
                                               groupSize[1] * groupCount[1],
                                               groupSize[2] * groupCount[2]};
    uint8_t size = 3 * sizeof(uint32_t);

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto pDispatchTraits = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(sizeof(ze_group_count_t), unifiedMemoryProperties);

    auto outBuffer = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);

    memset(outBuffer, 0, size);

    ze_group_count_t &dispatchTraits = *reinterpret_cast<ze_group_count_t *>(pDispatchTraits);
    dispatchTraits.groupCountX = groupCount[0];
    dispatchTraits.groupCountY = groupCount[1];
    dispatchTraits.groupCountZ = groupCount[2];

    ze_module_handle_t moduleHandle = createModuleFromFile("test_kernel", context, device, "");
    ASSERT_NE(nullptr, moduleHandle);
    ze_kernel_handle_t kernel;

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "test_get_global_sizes";
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(moduleHandle, &kernelDesc, &kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &outBuffer));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, groupSize[0], groupSize[1], groupSize[2]));
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelIndirect(cmdListHandle, kernel, &dispatchTraits, nullptr, 0, nullptr));
    commandList->close();

    pCmdq->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    pCmdq->synchronize(std::numeric_limits<uint32_t>::max());

    EXPECT_TRUE(csr->expectMemory(outBuffer, expectedGlobalWorkSize, size, aub_stream::CompareOperationValues::CompareEqual));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeModuleDestroy(moduleHandle));
    driverHandle->svmAllocsManager->freeSVMAlloc(outBuffer);
    driverHandle->svmAllocsManager->freeSVMAlloc(pDispatchTraits);
}

TEST_F(AUBAppendKernelIndirectL0, whenAppendKernelIndirectThenGroupCountIsProperlyProgrammed) {
    const uint32_t groupSize[] = {1, 2, 3};
    const uint32_t groupCount[] = {4, 3, 1};
    uint8_t size = 3 * sizeof(uint32_t);

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto pDispatchTraits = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(sizeof(ze_group_count_t), unifiedMemoryProperties);

    auto outBuffer = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);

    memset(outBuffer, 0, size);

    ze_group_count_t &dispatchTraits = *reinterpret_cast<ze_group_count_t *>(pDispatchTraits);
    dispatchTraits.groupCountX = groupCount[0];
    dispatchTraits.groupCountY = groupCount[1];
    dispatchTraits.groupCountZ = groupCount[2];

    ze_module_handle_t moduleHandle = createModuleFromFile("test_kernel", context, device, "");
    ASSERT_NE(nullptr, moduleHandle);
    ze_kernel_handle_t kernel;

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "test_get_group_count";
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(moduleHandle, &kernelDesc, &kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &outBuffer));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, groupSize[0], groupSize[1], groupSize[2]));
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelIndirect(cmdListHandle, kernel, &dispatchTraits, nullptr, 0, nullptr));
    commandList->close();

    pCmdq->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    pCmdq->synchronize(std::numeric_limits<uint32_t>::max());

    EXPECT_TRUE(csr->expectMemory(outBuffer, groupCount, size, aub_stream::CompareOperationValues::CompareEqual));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeModuleDestroy(moduleHandle));
    driverHandle->svmAllocsManager->freeSVMAlloc(outBuffer);
    driverHandle->svmAllocsManager->freeSVMAlloc(pDispatchTraits);
}

TEST_F(AUBAppendKernelIndirectL0, whenAppendMultipleKernelsIndirectThenGroupCountIsProperlyProgrammed) {
    const uint32_t groupSize[] = {1, 2, 3};
    const uint32_t groupCount[] = {4, 3, 1};
    const uint32_t groupCount2[] = {7, 6, 4};
    uint8_t size = 3 * sizeof(uint32_t);

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto pDispatchTraits = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(2 * sizeof(ze_group_count_t), unifiedMemoryProperties);

    auto kernelCount = reinterpret_cast<uint32_t *>(driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(sizeof(uint32_t), unifiedMemoryProperties));
    kernelCount[0] = 2;

    auto outBuffer = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    auto outBuffer2 = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);

    memset(outBuffer, 0, size);
    memset(outBuffer2, 0, size);

    ze_group_count_t *dispatchTraits = reinterpret_cast<ze_group_count_t *>(pDispatchTraits);
    dispatchTraits[0].groupCountX = groupCount[0];
    dispatchTraits[0].groupCountY = groupCount[1];
    dispatchTraits[0].groupCountZ = groupCount[2];

    dispatchTraits[1].groupCountX = groupCount2[0];
    dispatchTraits[1].groupCountY = groupCount2[1];
    dispatchTraits[1].groupCountZ = groupCount2[2];

    ze_module_handle_t moduleHandle = createModuleFromFile("test_kernel", context, device, "");
    ASSERT_NE(nullptr, moduleHandle);
    ze_kernel_handle_t kernels[2];

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "test_get_group_count";
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(moduleHandle, &kernelDesc, &kernels[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernels[0], 0, sizeof(void *), &outBuffer));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernels[0], groupSize[0], groupSize[1], groupSize[2]));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(moduleHandle, &kernelDesc, &kernels[1]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernels[1], 0, sizeof(void *), &outBuffer2));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernels[1], groupSize[0], groupSize[1], groupSize[2]));

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchMultipleKernelsIndirect(cmdListHandle, 2, kernels, kernelCount, dispatchTraits, nullptr, 0, nullptr));
    commandList->close();

    pCmdq->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    pCmdq->synchronize(std::numeric_limits<uint32_t>::max());

    EXPECT_TRUE(csr->expectMemory(outBuffer, groupCount, size, aub_stream::CompareOperationValues::CompareEqual));
    EXPECT_TRUE(csr->expectMemory(outBuffer2, groupCount2, size, aub_stream::CompareOperationValues::CompareEqual));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernels[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernels[1]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeModuleDestroy(moduleHandle));
    driverHandle->svmAllocsManager->freeSVMAlloc(outBuffer);
    driverHandle->svmAllocsManager->freeSVMAlloc(outBuffer2);
    driverHandle->svmAllocsManager->freeSVMAlloc(kernelCount);
    driverHandle->svmAllocsManager->freeSVMAlloc(pDispatchTraits);
}

TEST_F(AUBAppendKernelIndirectL0, whenAppendKernelIndirectThenWorkDimIsProperlyProgrammed) {
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    std::tuple<std::array<uint32_t, 3> /*groupSize*/, std::array<uint32_t, 3> /*groupCount*/, uint32_t /*expected workdim*/> testData[]{
        {{1, 1, 1}, {1, 1, 1}, 1},
        {{1, 1, 1}, {2, 1, 1}, 1},
        {{1, 1, 1}, {1, 2, 1}, 2},
        {{1, 1, 1}, {1, 1, 2}, 3},
        {{2, 1, 1}, {1, 1, 1}, 1},
        {{2, 1, 1}, {2, 1, 1}, 1},
        {{2, 1, 1}, {1, 2, 1}, 2},
        {{2, 1, 1}, {1, 1, 2}, 3},
        {{1, 2, 1}, {1, 1, 1}, 2},
        {{1, 2, 1}, {2, 1, 1}, 2},
        {{1, 2, 1}, {1, 2, 1}, 2},
        {{1, 2, 1}, {1, 1, 2}, 3},
        {{1, 1, 2}, {1, 1, 1}, 3},
        {{1, 1, 2}, {2, 1, 1}, 3},
        {{1, 1, 2}, {1, 2, 1}, 3},
        {{1, 1, 2}, {1, 1, 2}, 3}};

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    ze_module_handle_t moduleHandle = createModuleFromFile("test_kernel", context, device, "");
    ASSERT_NE(nullptr, moduleHandle);
    ze_kernel_handle_t kernel;

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "test_get_work_dim";
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(moduleHandle, &kernelDesc, &kernel));
    for (auto i = 0u; i < arrayCount<>(testData); i++) {
        std::array<uint32_t, 3> groupSize;
        std::array<uint32_t, 3> groupCount;
        uint32_t expectedWorkDim;

        std::tie(groupSize, groupCount, expectedWorkDim) = testData[i];

        uint8_t size = sizeof(uint32_t);

        auto pDispatchTraits = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(sizeof(ze_group_count_t), unifiedMemoryProperties);

        auto outBuffer = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);

        memset(outBuffer, 0, size);

        ze_group_count_t &dispatchTraits = *reinterpret_cast<ze_group_count_t *>(pDispatchTraits);
        dispatchTraits.groupCountX = groupCount[0];
        dispatchTraits.groupCountY = groupCount[1];
        dispatchTraits.groupCountZ = groupCount[2];

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &outBuffer));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, groupSize[0], groupSize[1], groupSize[2]));

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelIndirect(cmdListHandle, kernel, &dispatchTraits, nullptr, 0, nullptr));
        commandList->close();

        pCmdq->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
        pCmdq->synchronize(std::numeric_limits<uint32_t>::max());

        EXPECT_TRUE(csr->expectMemory(outBuffer, &expectedWorkDim, size, aub_stream::CompareOperationValues::CompareEqual));

        driverHandle->svmAllocsManager->freeSVMAlloc(outBuffer);
        driverHandle->svmAllocsManager->freeSVMAlloc(pDispatchTraits);
        commandList->reset();
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelDestroy(kernel));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeModuleDestroy(moduleHandle));
}

template <uint32_t TileCount>
struct BcsSplitAubFixture : public MulticontextL0AubFixture {
    virtual void setUp() {
        std::bitset<bcsInfoMaskSize> bcsMask = 0b110;

        if (blitterMaskOverride.has_value()) {
            bcsMask = blitterMaskOverride.value();
        }

        int32_t numEnabledEngines = static_cast<int32_t>(bcsMask.count());
        int32_t bcsMaskValue = static_cast<int32_t>(bcsMask.to_ulong());

        debugManager.flags.BlitterEnableMaskOverride.set(bcsMaskValue);
        debugManager.flags.SplitBcsCopy.set(1);
        debugManager.flags.SplitBcsRequiredTileCount.set(1);
        debugManager.flags.SplitBcsRequiredEnginesCount.set(numEnabledEngines);
        debugManager.flags.SplitBcsMask.set(bcsMaskValue);
        debugManager.flags.SplitBcsTransferDirectionMask.set(~(1 << static_cast<int32_t>(TransferDirection::localToLocal)));
        debugManager.flags.ContextGroupSize.set(0);

        if (static_cast<int32_t>(defaultHwInfo->featureTable.ftrBcsInfo.count()) < numEnabledEngines) {
            GTEST_SKIP();
        }

        this->dispatchMode = DispatchMode::immediateDispatch;
        MulticontextL0AubFixture::setUp(TileCount, EnabledCommandStreamers::single, (TileCount > 1));

        if (skipped) {
            GTEST_SKIP();
        }

        ze_context_handle_t hContext;
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        driverHandle->createContext(&desc, 0u, nullptr, &hContext);
        ASSERT_NE(nullptr, hContext);
        context.reset(static_cast<ContextImp *>(Context::fromHandle(hContext)));

        ze_result_t returnValue;
        commandList.reset(ult::CommandList::whiteboxCast(CommandList::create(rootDevice->getHwInfo().platform.eProductFamily, rootDevice, NEO::EngineGroupType::compute, 0u, returnValue, false)));
        ASSERT_NE(nullptr, commandList.get());

        ze_command_queue_desc_t queueDesc = {
            .ordinal = queryCopyOrdinal(),
            .flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER,
        };

        commandList.reset(CommandList::createImmediate(rootDevice->getHwInfo().platform.eProductFamily,
                                                       rootDevice,
                                                       &queueDesc,
                                                       false,
                                                       NEO::EngineGroupType::copy,
                                                       returnValue));

        ASSERT_NE(nullptr, commandList.get());
    }

    uint32_t queryCopyOrdinal() {
        uint32_t count = 0;
        rootDevice->getCommandQueueGroupProperties(&count, nullptr);

        std::vector<ze_command_queue_group_properties_t> groups;
        groups.resize(count);

        rootDevice->getCommandQueueGroupProperties(&count, groups.data());

        for (uint32_t i = 0; i < count; i++) {
            if (groups[i].flags == ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) {
                return i;
            }
        }

        EXPECT_TRUE(false);
        return 0;
    }

    DestroyableZeUniquePtr<Event> createAggregatedEvent(uint64_t incValue, uint64_t completionValue) {
        ze_device_mem_alloc_desc_t deviceDesc = {};
        void *devAddress = nullptr;
        context->allocDeviceMem(rootDevice->toHandle(), &deviceDesc, sizeof(uint64_t), 4096u, &devAddress);
        ze_event_handle_t outEvent = nullptr;
        zex_counter_based_event_external_storage_properties_t externalStorageAllocProperties = {
            .stype = ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_STORAGE_ALLOC_PROPERTIES,
            .deviceAddress = reinterpret_cast<uint64_t *>(devAddress),
            .incrementValue = incValue,
            .completionValue = completionValue,
        };

        zex_counter_based_event_desc_t counterBasedDesc = {
            .stype = ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC,
            .pNext = &externalStorageAllocProperties,
            .flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE,
        };

        EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context.get(), rootDevice, &counterBasedDesc, &outEvent));

        return DestroyableZeUniquePtr<Event>(Event::fromHandle(outEvent));
    }

    DestroyableZeUniquePtr<ContextImp> context;
    DestroyableZeUniquePtr<L0::CommandList> commandList;
};

using BcsSplitAubTests = Test<BcsSplitAubFixture<1>>;

HWTEST2_F(BcsSplitAubTests, whenAppendingCopyWithAggregatedEventThenEventIsSignaledAndDataIsCorrect, IsAtLeastXeHpcCore) {
    auto whiteboxCmdList = static_cast<ult::WhiteBox<L0::CommandListImp> *>(commandList.get());
    if (!whiteboxCmdList->isBcsSplitNeeded) {
        GTEST_SKIP();
    }

    size_t bufferSize = whiteboxCmdList->minimalSizeForBcsSplit;
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto srcBuffer = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(bufferSize, unifiedMemoryProperties);
    auto dstBuffer1 = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(bufferSize, unifiedMemoryProperties);
    auto dstBuffer2 = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(bufferSize, unifiedMemoryProperties);

    std::fill(static_cast<uint8_t *>(srcBuffer), ptrOffset(static_cast<uint8_t *>(srcBuffer), bufferSize), 1);
    memset(dstBuffer1, 0, bufferSize);
    memset(dstBuffer2, 0, bufferSize);

    uint32_t incValue = 0;
    zexDeviceGetAggregatedCopyOffloadIncrementValue(rootDevice->toHandle(), &incValue);
    auto event = createAggregatedEvent(incValue, incValue * 2);
    auto eventStorage = reinterpret_cast<uint64_t *>(event->getInOrderExecInfo()->getBaseDeviceAddress());
    *event->getInOrderExecInfo()->getBaseHostAddress() = 0;

    auto csr = getRootSimulatedCsr<FamilyType>();
    csr->writeMemory(*event->getInOrderExecInfo()->getDeviceCounterAllocation());

    zeCommandListAppendMemoryCopy(commandList->toHandle(), dstBuffer1, srcBuffer, bufferSize, event->toHandle(), 0, nullptr);
    zeCommandListHostSynchronize(commandList->toHandle(), std::numeric_limits<uint64_t>::max());

    auto bcsSplit = static_cast<DeviceImp *>(rootDevice)->bcsSplit.get();
    ASSERT_NE(nullptr, bcsSplit);

    auto whiteboxSplitCmdList = static_cast<ult::WhiteBox<L0::CommandListImp> *>(bcsSplit->cmdLists[0]);

    auto taskCount = whiteboxSplitCmdList->cmdQImmediate->getTaskCount();
    EXPECT_TRUE(taskCount >= 1);

    auto expectedIncValue = static_cast<uint64_t>(incValue);
    EXPECT_TRUE(csr->expectMemory(dstBuffer1, srcBuffer, bufferSize, aub_stream::CompareOperationValues::CompareEqual));
    EXPECT_TRUE(csr->expectMemory(eventStorage, &expectedIncValue, sizeof(uint64_t), aub_stream::CompareOperationValues::CompareEqual));

    zeCommandListAppendMemoryCopy(commandList->toHandle(), dstBuffer2, srcBuffer, bufferSize, event->toHandle(), 0, nullptr);
    zeCommandListHostSynchronize(commandList->toHandle(), std::numeric_limits<uint64_t>::max());

    EXPECT_TRUE(whiteboxSplitCmdList->cmdQImmediate->getTaskCount() > taskCount);

    expectedIncValue = incValue * 2;
    EXPECT_TRUE(csr->expectMemory(dstBuffer2, srcBuffer, bufferSize, aub_stream::CompareOperationValues::CompareEqual));
    EXPECT_TRUE(csr->expectMemory(eventStorage, &expectedIncValue, sizeof(uint64_t), aub_stream::CompareOperationValues::CompareEqual));

    driverHandle->svmAllocsManager->freeSVMAlloc(srcBuffer);
    driverHandle->svmAllocsManager->freeSVMAlloc(dstBuffer1);
    driverHandle->svmAllocsManager->freeSVMAlloc(dstBuffer2);
    context->freeMem(eventStorage);
}

using BcsSplitMultitileAubTests = Test<BcsSplitAubFixture<2>>;

HWTEST2_F(BcsSplitMultitileAubTests, whenAppendingCopyWithAggregatedEventThenEventIsSignaledAndDataIsCorrect, IsAtLeastXeHpcCore) {
    if (!rootDevice->isImplicitScalingCapable()) {
        GTEST_SKIP();
    }

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER | ZE_COMMAND_QUEUE_FLAG_COPY_OFFLOAD_HINT};
    commandList.reset(CommandList::createImmediate(rootDevice->getHwInfo().platform.eProductFamily, rootDevice, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    ASSERT_NE(nullptr, commandList.get());

    auto whiteboxCmdList = static_cast<ult::WhiteBox<L0::CommandListImp> *>(commandList.get());
    EXPECT_TRUE(whiteboxCmdList->isCopyOffloadEnabled());
    EXPECT_TRUE(whiteboxCmdList->isBcsSplitNeeded);

    size_t bufferSize = whiteboxCmdList->minimalSizeForBcsSplit;
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto srcBuffer = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(bufferSize, unifiedMemoryProperties);
    auto dstBuffer1 = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(bufferSize, unifiedMemoryProperties);
    auto dstBuffer2 = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(bufferSize, unifiedMemoryProperties);

    std::fill(static_cast<uint8_t *>(srcBuffer), ptrOffset(static_cast<uint8_t *>(srcBuffer), bufferSize), 1);
    memset(dstBuffer1, 0, bufferSize);
    memset(dstBuffer2, 0, bufferSize);

    uint32_t incValue = 0;
    zexDeviceGetAggregatedCopyOffloadIncrementValue(rootDevice->toHandle(), &incValue);
    auto event = createAggregatedEvent(incValue, incValue * 2);
    auto eventStorage = reinterpret_cast<uint64_t *>(event->getInOrderExecInfo()->getBaseDeviceAddress());
    *event->getInOrderExecInfo()->getBaseHostAddress() = 0;

    auto csr = getRootSimulatedCsr<FamilyType>();
    csr->writeMemory(*event->getInOrderExecInfo()->getDeviceCounterAllocation());

    zeCommandListAppendMemoryCopy(commandList->toHandle(), dstBuffer1, srcBuffer, bufferSize, event->toHandle(), 0, nullptr);
    zeCommandListHostSynchronize(commandList->toHandle(), std::numeric_limits<uint64_t>::max());

    auto bcsSplit = static_cast<DeviceImp *>(rootDevice)->bcsSplit.get();
    ASSERT_NE(nullptr, bcsSplit);

    auto whiteboxSplitCmdList = static_cast<ult::WhiteBox<L0::CommandListImp> *>(bcsSplit->cmdLists[0]);

    auto taskCount = whiteboxSplitCmdList->cmdQImmediate->getTaskCount();
    EXPECT_TRUE(taskCount >= 1);

    auto expectedIncValue = static_cast<uint64_t>(incValue);
    EXPECT_TRUE(csr->expectMemory(dstBuffer1, srcBuffer, bufferSize, aub_stream::CompareOperationValues::CompareEqual));
    EXPECT_TRUE(csr->expectMemory(eventStorage, &expectedIncValue, sizeof(uint64_t), aub_stream::CompareOperationValues::CompareEqual));

    zeCommandListAppendMemoryCopy(commandList->toHandle(), dstBuffer2, srcBuffer, bufferSize, event->toHandle(), 0, nullptr);
    zeCommandListHostSynchronize(commandList->toHandle(), std::numeric_limits<uint64_t>::max());

    EXPECT_TRUE(whiteboxSplitCmdList->cmdQImmediate->getTaskCount() > taskCount);

    expectedIncValue = incValue * 2;
    EXPECT_TRUE(csr->expectMemory(dstBuffer2, srcBuffer, bufferSize, aub_stream::CompareOperationValues::CompareEqual));
    EXPECT_TRUE(csr->expectMemory(eventStorage, &expectedIncValue, sizeof(uint64_t), aub_stream::CompareOperationValues::CompareEqual));

    driverHandle->svmAllocsManager->freeSVMAlloc(srcBuffer);
    driverHandle->svmAllocsManager->freeSVMAlloc(dstBuffer1);
    driverHandle->svmAllocsManager->freeSVMAlloc(dstBuffer2);
    context->freeMem(eventStorage);
}

} // namespace ult
} // namespace L0
