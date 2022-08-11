/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

class CommandListFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();
        ze_result_t returnValue;
        commandList.reset(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        eventPoolDesc.count = 2;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.wait = 0;
        eventDesc.signal = 0;

        eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
        event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    }

    void tearDown() {
        event.reset(nullptr);
        eventPool.reset(nullptr);
        commandList.reset(nullptr);
        DeviceFixture::tearDown();
    }

    std::unique_ptr<L0::ult::CommandList> commandList;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;
};

template <bool createImmediate, bool createInternal, bool createCopy>
struct MultiTileCommandListFixture : public SingleRootMultiSubDeviceFixture {
    void setUp() {
        DebugManager.flags.EnableImplicitScaling.set(1);
        osLocalMemoryBackup = std::make_unique<VariableBackup<bool>>(&NEO::OSInterface::osEnableLocalMemory, true);
        apiSupportBackup = std::make_unique<VariableBackup<bool>>(&NEO::ImplicitScaling::apiSupport, true);

        SingleRootMultiSubDeviceFixture::setUp();
        ze_result_t returnValue;

        NEO::EngineGroupType cmdListEngineType = createCopy ? NEO::EngineGroupType::Copy : NEO::EngineGroupType::RenderCompute;

        if (!createImmediate) {
            commandList.reset(whiteboxCast(CommandList::create(productFamily, device, cmdListEngineType, 0u, returnValue)));
        } else {
            const ze_command_queue_desc_t desc = {};
            commandList.reset(whiteboxCast(CommandList::createImmediate(productFamily, device, &desc, createInternal, cmdListEngineType, returnValue)));
        }
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        eventPoolDesc.count = 2;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.wait = 0;
        eventDesc.signal = 0;

        eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
        event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    }

    void tearDown() {
        SingleRootMultiSubDeviceFixture::tearDown();
    }

    std::unique_ptr<L0::ult::CommandList> commandList;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;
    std::unique_ptr<VariableBackup<bool>> apiSupportBackup;
    std::unique_ptr<VariableBackup<bool>> osLocalMemoryBackup;
};

template <typename FamilyType>
void validateTimestampRegisters(GenCmdList &cmdList,
                                GenCmdList::iterator &startIt,
                                uint32_t firstLoadRegisterRegSrcAddress,
                                uint64_t firstStoreRegMemAddress,
                                uint32_t secondLoadRegisterRegSrcAddress,
                                uint64_t secondStoreRegMemAddress,
                                bool workloadPartition) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    constexpr uint32_t mask = 0xfffffffe;

    auto itor = find<MI_LOAD_REGISTER_REG *>(startIt, cmdList.end());

    {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdLoadReg = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(firstLoadRegisterRegSrcAddress, cmdLoadReg->getSourceRegisterAddress());
        EXPECT_EQ(CS_GPR_R0, cmdLoadReg->getDestinationRegisterAddress());
    }

    itor++;
    {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdLoadImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
        EXPECT_EQ(CS_GPR_R1, cmdLoadImm->getRegisterOffset());
        EXPECT_EQ(mask, cmdLoadImm->getDataDword());
    }

    itor++;
    {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdMath = genCmdCast<MI_MATH *>(*itor);
        EXPECT_EQ(3u, cmdMath->DW0.BitField.DwordLength);
    }

    itor++;
    {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(CS_GPR_R2, cmdMem->getRegisterAddress());
        EXPECT_EQ(firstStoreRegMemAddress, cmdMem->getMemoryAddress());
        if (workloadPartition) {
            EXPECT_TRUE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        } else {
            EXPECT_FALSE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        }
    }

    itor++;
    {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdLoadReg = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(secondLoadRegisterRegSrcAddress, cmdLoadReg->getSourceRegisterAddress());
        EXPECT_EQ(CS_GPR_R0, cmdLoadReg->getDestinationRegisterAddress());
    }

    itor++;
    {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdLoadImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
        EXPECT_EQ(CS_GPR_R1, cmdLoadImm->getRegisterOffset());
        EXPECT_EQ(mask, cmdLoadImm->getDataDword());
    }

    itor++;
    {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdMath = genCmdCast<MI_MATH *>(*itor);
        EXPECT_EQ(3u, cmdMath->DW0.BitField.DwordLength);
    }

    itor++;
    {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(CS_GPR_R2, cmdMem->getRegisterAddress());
        EXPECT_EQ(secondStoreRegMemAddress, cmdMem->getMemoryAddress());
        if (workloadPartition) {
            EXPECT_TRUE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        } else {
            EXPECT_FALSE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        }
    }
    itor++;
    startIt = itor;
}

} // namespace ult
} // namespace L0
