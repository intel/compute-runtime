/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

namespace L0 {
namespace ult {

class CommandListMemoryExtensionFixture : public DeviceFixture {
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
        eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
        eventDesc.signal = 0;

        eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
        event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

        size_t size = sizeof(uint32_t);
        size_t alignment = 1u;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        auto result = context->allocDeviceMem(device->toHandle(),
                                              &deviceDesc,
                                              size, alignment, &ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptr);
    }

    void TearDown() {
        context->freeMem(ptr);
        DeviceFixture::TearDown();
    }

    std::unique_ptr<L0::ult::CommandList> commandList;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;
    uint32_t waitMemData = 1u;
    void *ptr = nullptr;
};

using CommandListAppendWaitOnMemExtension = Test<CommandListMemoryExtensionFixture>;

TEST_F(CommandListAppendWaitOnMemExtension, givenAppendWaitOnMemReturnsUnsupported) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    result = commandList->appendWaitOnMemory(nullptr, nullptr, 1u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

using CommandListAppendWriteToMemExtension = Test<CommandListMemoryExtensionFixture>;

TEST_F(CommandListAppendWriteToMemExtension, givenAppendWriteToMemReturnsUnsupported) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    uint64_t data = 0xabc;
    result = commandList->appendWriteToMemory(nullptr, nullptr, data);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

} // namespace ult
} // namespace L0
