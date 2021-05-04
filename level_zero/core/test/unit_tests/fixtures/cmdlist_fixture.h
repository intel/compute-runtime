/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

class CommandListFixture : public DeviceFixture {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        ze_result_t returnValue;
        commandList.reset(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue)));

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        eventPoolDesc.count = 2;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.wait = 0;
        eventDesc.signal = 0;

        eventPool = EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc);
        event = Event::create(eventPool, &eventDesc, device);
    }

    void TearDown() override {
        if (event) {
            event->destroy();
        }
        if (eventPool) {
            eventPool->destroy();
        }
        DeviceFixture::TearDown();
    }

    std::unique_ptr<L0::ult::CommandList> commandList;
    EventPool *eventPool = nullptr;
    Event *event = nullptr;
};

} // namespace ult
} // namespace L0
