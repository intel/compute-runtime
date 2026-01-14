/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/experimental/source/graph/graph.h"

using namespace NEO;

namespace L0 {

namespace ult {

struct GraphsCleanupGuard {
    ~GraphsCleanupGuard() {
        processUsesGraphs.store(false);
    }
};

struct MockGraphCmdListWithContext : Mock<CommandList> {
    using WhiteBox<::L0::CommandListImp>::cmdListType;

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

struct MockGraphContextReturningSpecificCmdList : Mock<Context> {
    std::vector<Mock<CommandList> *> cmdListsToReturn;
    ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList) override {
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

struct MockGraphContextReturningNewCmdList : Mock<Context> {
    ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList) override {
        *commandList = new Mock<CommandList>;
        return ZE_RESULT_SUCCESS;
    }
};

} // namespace ult
} // namespace L0
