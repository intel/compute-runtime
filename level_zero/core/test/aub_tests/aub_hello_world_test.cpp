/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include "test_mode.h"

namespace L0 {
namespace ult {

using AUBHelloWorldL0 = Test<AUBFixtureL0>;
TEST_F(AUBHelloWorldL0, whenAppendMemoryCopyIsCalledThenMemoryIsProperlyCopied) {
    uint8_t size = 8;
    uint8_t val = 255;

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto srcMemory = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    auto dstMemory = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);

    memset(srcMemory, val, size);
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryCopy(dstMemory, srcMemory, size, nullptr, 0, nullptr, copyParams);
    commandList->close();
    auto pHCmdList = std::make_unique<ze_command_list_handle_t>(commandList->toHandle());

    pCmdq->executeCommandLists(1, pHCmdList.get(), nullptr, false, nullptr, nullptr);
    pCmdq->synchronize(std::numeric_limits<uint32_t>::max());

    EXPECT_TRUE(csr->expectMemory(dstMemory, srcMemory, size, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));

    driverHandle->svmAllocsManager->freeSVMAlloc(srcMemory);
    driverHandle->svmAllocsManager->freeSVMAlloc(dstMemory);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            AUBHelloWorldL0,
            GivenPatchPreambleQueueAndTwoCommandListsWhenAppendMemoryCopyFromSourceToStageAndFromStageToDestinationThenDataIsCopied) {
    constexpr size_t size = 64;
    constexpr uint8_t val1 = 255;
    constexpr uint8_t val2 = 127;

    ze_result_t returnValue;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    auto contextHandle = context->toHandle();
    auto deviceHandle = device->toHandle();

    void *srcMemory1 = nullptr;
    void *dstMemory1 = nullptr;
    void *srcMemory2 = nullptr;
    void *dstMemory2 = nullptr;
    void *stageMemory = nullptr;

    returnValue = zeMemAllocHost(contextHandle, &hostDesc, size, 1, &srcMemory1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    memset(srcMemory1, val1, size);
    returnValue = zeMemAllocHost(contextHandle, &hostDesc, size, 1, &srcMemory2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    memset(srcMemory2, val2, size);
    returnValue = zeMemAllocHost(contextHandle, &hostDesc, size, 1, &dstMemory1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemAllocHost(contextHandle, &hostDesc, size, 1, &dstMemory2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    memset(dstMemory1, 0, size);
    memset(dstMemory2, 0, size);
    returnValue = zeMemAllocDevice(contextHandle, &deviceDesc, size, 1, deviceHandle, &stageMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_event_pool_handle_t eventPoolHandle = nullptr;
    ze_event_handle_t eventHandle = nullptr;

    ze_event_pool_desc_t eventPoolDesc{ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 1;

    returnValue = zeEventPoolCreate(contextHandle, &eventPoolDesc, 0, nullptr, &eventPoolHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_event_desc_t eventDesc{ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = 0;

    returnValue = zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_command_list_desc_t commandListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};

    ze_command_list_handle_t commandListHandle = nullptr;
    // create command list and copy from srcMemory1 to stageMemory - signal event
    returnValue = zeCommandListCreate(contextHandle, deviceHandle, &commandListDesc, &commandListHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListAppendMemoryCopy(commandListHandle, stageMemory, srcMemory1, size, eventHandle, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListClose(commandListHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    // create second command list and copy from stageMemory to dstMemory1 - wait for event
    ze_command_list_handle_t commandListHandle2 = nullptr;
    returnValue = zeCommandListCreate(contextHandle, deviceHandle, &commandListDesc, &commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListAppendMemoryCopy(commandListHandle2, dstMemory1, stageMemory, size, nullptr, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListClose(commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_command_list_handle_t commandLists[] = {commandListHandle, commandListHandle2};

    uint32_t commandListCount = sizeof(commandLists) / sizeof(commandLists[0]);

    pCmdq->setPatchingPreamble(true);

    auto queueHandle = pCmdq->toHandle();

    // execute command lists and program expected memory srcMemory1 to dstMemory1
    returnValue = zeCommandQueueExecuteCommandLists(queueHandle, commandListCount, commandLists, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandQueueSynchronize(queueHandle, std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_TRUE(csr->expectMemory(dstMemory1, srcMemory1, size, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));

    // reset command lists and event
    returnValue = zeCommandListReset(commandListHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListReset(commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeEventHostReset(eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    // copy from srcMemory2 to stageMemory - signal event
    returnValue = zeCommandListAppendMemoryCopy(commandListHandle, stageMemory, srcMemory2, size, eventHandle, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListClose(commandListHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    // copy from stageMemory to dstMemory2 - wait for event
    returnValue = zeCommandListAppendMemoryCopy(commandListHandle2, dstMemory2, stageMemory, size, nullptr, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListClose(commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    // execute command lists and program expected memory srcMemory2 to dstMemory2
    returnValue = zeCommandQueueExecuteCommandLists(queueHandle, commandListCount, commandLists, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandQueueSynchronize(queueHandle, std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_TRUE(csr->expectMemory(dstMemory2, srcMemory2, size, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));

    // destroy resources
    returnValue = zeEventDestroy(eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeEventPoolDestroy(eventPoolHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemFree(contextHandle, srcMemory1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemFree(contextHandle, srcMemory2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemFree(contextHandle, dstMemory1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemFree(contextHandle, dstMemory2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemFree(contextHandle, stageMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListDestroy(commandListHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListDestroy(commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

} // namespace ult
} // namespace L0
