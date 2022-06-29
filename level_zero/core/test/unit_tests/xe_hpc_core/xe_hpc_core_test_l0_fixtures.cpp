/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/xe_hpc_core/xe_hpc_core_test_l0_fixtures.h"

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

using namespace L0;
using namespace ult;

void DeviceFixtureXeHpcTests::checkIfCallingGetMemoryPropertiesWithNonNullPtrThenMaxClockRateReturnZero(HardwareInfo *hwInfo) {
    uint32_t count = 0;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    ze_device_memory_properties_t memProperties = {};
    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    EXPECT_EQ(memProperties.maxClockRate, 0u);
}

void CommandListStatePrefetchXeHpcCore::checkIfDebugFlagSetWhenPrefetchApiCalledAThenStatePrefetchProgrammed(HardwareInfo *hwInfo) {
    using STATE_PREFETCH = typename XE_HPC_COREFamily::STATE_PREFETCH;
    DebugManagerStateRestore restore;
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<IGFX_XE_HPC_CORE>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    constexpr size_t size = MemoryConstants::cacheLineSize * 2;
    constexpr size_t alignment = MemoryConstants::pageSize64k;
    constexpr size_t offset = MemoryConstants::cacheLineSize;
    constexpr uint32_t mocsIndexForL3 = (2 << 1);
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(), &deviceDesc, size + offset, alignment, &ptr);
    EXPECT_NE(nullptr, ptr);

    auto cmdListBaseOffset = pCommandList->commandContainer.getCommandStream()->getUsed();

    {
        auto ret = pCommandList->appendMemoryPrefetch(ptrOffset(ptr, offset), size);
        EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

        EXPECT_EQ(cmdListBaseOffset, pCommandList->commandContainer.getCommandStream()->getUsed());
    }

    {
        DebugManager.flags.AddStatePrefetchCmdToMemoryPrefetchAPI.set(1);

        auto ret = pCommandList->appendMemoryPrefetch(ptrOffset(ptr, offset), size);
        EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

        EXPECT_EQ(cmdListBaseOffset + sizeof(STATE_PREFETCH), pCommandList->commandContainer.getCommandStream()->getUsed());

        auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(ptrOffset(pCommandList->commandContainer.getCommandStream()->getCpuBase(), cmdListBaseOffset));

        EXPECT_EQ(statePrefetchCmd->getAddress(), reinterpret_cast<uint64_t>(ptrOffset(ptr, offset)));
        EXPECT_FALSE(statePrefetchCmd->getKernelInstructionPrefetch());
        EXPECT_EQ(mocsIndexForL3, statePrefetchCmd->getMemoryObjectControlState());
        EXPECT_EQ(1u, statePrefetchCmd->getPrefetchSize());

        EXPECT_EQ(reinterpret_cast<uint64_t>(ptr), pCommandList->commandContainer.getResidencyContainer().back()->getGpuAddress());
    }

    context->freeMem(ptr);
}

void CommandListStatePrefetchXeHpcCore::checkIfCommandBufferIsExhaustedWhenPrefetchApiCalledThenStatePrefetchProgrammed(HardwareInfo *hwInfo) {
    using STATE_PREFETCH = typename XE_HPC_COREFamily::STATE_PREFETCH;
    using MI_BATCH_BUFFER_END = typename XE_HPC_COREFamily::MI_BATCH_BUFFER_END;

    DebugManagerStateRestore restore;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<IGFX_XE_HPC_CORE>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    constexpr size_t size = MemoryConstants::cacheLineSize * 2;
    constexpr size_t alignment = MemoryConstants::pageSize64k;
    constexpr size_t offset = MemoryConstants::cacheLineSize;
    constexpr uint32_t mocsIndexForL3 = (2 << 1);
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(), &deviceDesc, size + offset, alignment, &ptr);
    EXPECT_NE(nullptr, ptr);

    auto firstBatchBufferAllocation = pCommandList->commandContainer.getCommandStream()->getGraphicsAllocation();

    auto useSize = pCommandList->commandContainer.getCommandStream()->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    pCommandList->commandContainer.getCommandStream()->getSpace(useSize);

    DebugManager.flags.AddStatePrefetchCmdToMemoryPrefetchAPI.set(1);

    auto ret = pCommandList->appendMemoryPrefetch(ptrOffset(ptr, offset), size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    auto secondBatchBufferAllocation = pCommandList->commandContainer.getCommandStream()->getGraphicsAllocation();

    EXPECT_NE(firstBatchBufferAllocation, secondBatchBufferAllocation);

    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(pCommandList->commandContainer.getCommandStream()->getCpuBase());

    EXPECT_EQ(statePrefetchCmd->getAddress(), reinterpret_cast<uint64_t>(ptrOffset(ptr, offset)));
    EXPECT_FALSE(statePrefetchCmd->getKernelInstructionPrefetch());
    EXPECT_EQ(mocsIndexForL3, statePrefetchCmd->getMemoryObjectControlState());
    EXPECT_EQ(1u, statePrefetchCmd->getPrefetchSize());

    NEO::ResidencyContainer::iterator it = pCommandList->commandContainer.getResidencyContainer().end();
    it--;
    EXPECT_EQ(secondBatchBufferAllocation->getGpuAddress(), (*it)->getGpuAddress());
    it--;
    EXPECT_EQ(reinterpret_cast<uint64_t>(ptr), (*it)->getGpuAddress());

    context->freeMem(ptr);
}
