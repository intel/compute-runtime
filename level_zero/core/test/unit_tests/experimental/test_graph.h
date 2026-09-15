/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/internal/l0_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/experimental/source/graph/graph.h"
#include "level_zero/ze_api.h"

using namespace NEO;

namespace L0 {

namespace ult {

struct GraphsCleanupGuard {
    ~GraphsCleanupGuard() {
        processUsesGraphs.store(false);
    }
};

inline ze_event_handle_t createCounterBasedEvent(L0::Context *context, L0::Device *device, bool graphExternal) {
    ze_event_handle_t eventHandle = nullptr;
    ze_event_counter_based_desc_t eventDesc = {
        .stype = ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC,
        .flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE};
    if (graphExternal) {
        eventDesc.flags |= ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));
    return eventHandle;
}

struct MockGraphCmdListWithContext : Mock<CommandList> {
    using WhiteBox<::L0::CommandList>::cmdListType;

    MockGraphCmdListWithContext(L0::Context *ctx) : ctx(ctx) {
        cmdListType = ::L0::CommandList::CommandListType::typeImmediate;
    }
    ze_result_t getContextHandle(ze_context_handle_t *phContext) override {
        *phContext = ctx;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getDeviceHandle(ze_device_handle_t *phDevice) override {
        *phDevice = getDevice();
        return ZE_RESULT_SUCCESS;
    }

    L0::Context *ctx = nullptr;
};

struct MockGraphContextReturningSpecificCmdList : ContextStubMock {
    std::vector<Mock<CommandList> *> cmdListsToReturn;
    std::vector<uint32_t> estimatedNumberOfCommandsPerCreate;
    DriverHandle *driverHandleToReturn = nullptr;

    DriverHandle *getDriverHandle() override {
        return driverHandleToReturn;
    }

    ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList, uint32_t estimatedNumberOfCommands) override {
        UNRECOVERABLE_IF(cmdListsToReturn.empty());
        estimatedNumberOfCommandsPerCreate.push_back(estimatedNumberOfCommands);
        *commandList = cmdListsToReturn.front();
        cmdListsToReturn.erase(cmdListsToReturn.begin());
        return ZE_RESULT_SUCCESS;
    }

    MockGraphContextReturningSpecificCmdList() = default;
    MockGraphContextReturningSpecificCmdList(const MockGraphContextReturningSpecificCmdList &) = delete;
    MockGraphContextReturningSpecificCmdList &operator=(const MockGraphContextReturningSpecificCmdList &) = delete;
    MockGraphContextReturningSpecificCmdList(MockGraphContextReturningSpecificCmdList &&) = delete;
    MockGraphContextReturningSpecificCmdList &operator=(MockGraphContextReturningSpecificCmdList &&) = delete;
    ~MockGraphContextReturningSpecificCmdList() override {
        for (auto &cmdList : cmdListsToReturn) {
            delete static_cast<L0::CommandList *>(cmdList);
        }
    }
};

struct MockGraphContextReturningNewCmdList : ContextStubMock {
    DriverHandle *driverHandleToReturn = nullptr;

    DriverHandle *getDriverHandle() override {
        return driverHandleToReturn;
    }

    ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList, uint32_t estimatedNumberOfCommands) override {
        *commandList = new Mock<CommandList>;
        return ZE_RESULT_SUCCESS;
    }
};

} // namespace ult
} // namespace L0
