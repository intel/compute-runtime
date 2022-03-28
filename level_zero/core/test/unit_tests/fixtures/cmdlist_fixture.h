/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

class CommandListFixture : public DeviceFixture {
  public:
    void SetUp() {
        DeviceFixture::SetUp();
        ze_result_t returnValue;
        commandList.reset(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));

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

    void TearDown() {
        DeviceFixture::TearDown();
    }

    std::unique_ptr<L0::ult::CommandList> commandList;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;
};

template <bool createImmediate, bool createInternal, bool createCopy>
struct MultiTileCommandListFixture : public SingleRootMultiSubDeviceFixture {
    void SetUp() {
        DebugManager.flags.EnableImplicitScaling.set(1);
        osLocalMemoryBackup = std::make_unique<VariableBackup<bool>>(&NEO::OSInterface::osEnableLocalMemory, true);
        apiSupportBackup = std::make_unique<VariableBackup<bool>>(&NEO::ImplicitScaling::apiSupport, true);

        SingleRootMultiSubDeviceFixture::SetUp();
        ze_result_t returnValue;

        NEO::EngineGroupType cmdListEngineType = createCopy ? NEO::EngineGroupType::Copy : NEO::EngineGroupType::RenderCompute;

        if (!createImmediate) {
            commandList.reset(whitebox_cast(CommandList::create(productFamily, device, cmdListEngineType, 0u, returnValue)));
        } else {
            const ze_command_queue_desc_t desc = {};
            commandList.reset(whitebox_cast(CommandList::createImmediate(productFamily, device, &desc, createInternal, cmdListEngineType, returnValue)));
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

    void TearDown() {
        SingleRootMultiSubDeviceFixture::TearDown();
    }

    std::unique_ptr<L0::ult::CommandList> commandList;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;
    std::unique_ptr<VariableBackup<bool>> apiSupportBackup;
    std::unique_ptr<VariableBackup<bool>> osLocalMemoryBackup;
};

} // namespace ult
} // namespace L0
