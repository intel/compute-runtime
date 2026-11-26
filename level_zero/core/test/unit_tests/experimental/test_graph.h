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
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_cmdlist.h"
#include "level_zero/experimental/source/graph/graph.h"
#include "level_zero/ze_api.h"

#include <vector>

using namespace NEO;

namespace L0 {

namespace ult {

struct GraphsCleanupGuard {
    ~GraphsCleanupGuard() {
        processUsesGraphs.store(false);
    }
};

inline ze_event_handle_t createCounterBasedEvent(L0::Context *context, L0::Device *device, bool graphExternal, ze_event_counter_based_flags_t extraFlags) {
    ze_event_handle_t eventHandle = nullptr;
    ze_event_counter_based_desc_t eventDesc = {
        .stype = ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC,
        .flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | extraFlags};
    if (graphExternal) {
        eventDesc.flags |= ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));
    return eventHandle;
}

inline ze_event_handle_t createCounterBasedEvent(L0::Context *context, L0::Device *device, bool graphExternal) {
    return createCounterBasedEvent(context, device, graphExternal, 0);
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

template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandIdRecordingCommandList : Mock<CommandList> {
    struct RecordingMutableCommandList : MutableCommandListCoreFamily<gfxCoreFamily> {
        struct GraphNodeUpdate {
            uint64_t commandId = 0;
            ze_mutable_graph_node_exp_flags_t flags = 0;
            const void *pUpdateDesc = nullptr;
        };

        ze_result_t getNextCommandIdForGraphNode(ze_mutable_graph_node_exp_flags_t flags, uint32_t numKernels, ze_kernel_handle_t *phKernels, uint64_t *pCommandId) override {
            recordedFlags.push_back(flags);
            if (ZE_RESULT_SUCCESS != getNextCommandIdResult) {
                return getNextCommandIdResult;
            }
            *pCommandId = ++getNextCommandIdCalled;
            return ZE_RESULT_SUCCESS;
        }

        ze_result_t updateMutableCommandForGraphNode(uint64_t commandId, ze_mutable_graph_node_exp_flags_t flags, const void *pUpdateDesc) override {
            recordedGraphNodeUpdates.push_back({commandId, flags, pUpdateDesc});
            return updateMutableCommandForGraphNodeResult;
        }

        auto applyGraphNodeMutation(const MCL::KernelArgumentMutationDescriptor &mutation) -> ze_result_t override {
            recordedKernelArgumentMutations.push_back(mutation);
            return applyGraphNodeMutationResult;
        }

        auto applyGraphNodeMutation(const MCL::GroupCountMutationDescriptor &mutation) -> ze_result_t override {
            recordedGroupCountMutations.push_back(mutation);
            return applyGraphNodeMutationResult;
        }

        auto applyGraphNodeMutation(const MCL::GroupSizeMutationDescriptor &mutation) -> ze_result_t override {
            recordedGroupSizeMutations.push_back(mutation);
            return applyGraphNodeMutationResult;
        }

        auto applyGraphNodeMutation(const MCL::GlobalOffsetMutationDescriptor &mutation) -> ze_result_t override {
            recordedGlobalOffsetMutations.push_back(mutation);
            return applyGraphNodeMutationResult;
        }

        uint64_t getNextCommandIdCalled = 0;
        std::vector<ze_mutable_graph_node_exp_flags_t> recordedFlags;
        ze_result_t getNextCommandIdResult = ZE_RESULT_SUCCESS;
        std::vector<GraphNodeUpdate> recordedGraphNodeUpdates;
        ze_result_t updateMutableCommandForGraphNodeResult = ZE_RESULT_SUCCESS;
        std::vector<MCL::KernelArgumentMutationDescriptor> recordedKernelArgumentMutations;
        std::vector<MCL::GroupCountMutationDescriptor> recordedGroupCountMutations;
        std::vector<MCL::GroupSizeMutationDescriptor> recordedGroupSizeMutations;
        std::vector<MCL::GlobalOffsetMutationDescriptor> recordedGlobalOffsetMutations;
        ze_result_t applyGraphNodeMutationResult = ZE_RESULT_SUCCESS;
    } mutableCommandList;

    explicit CommandIdRecordingCommandList(L0::Device *device) {
        this->device = device;
        this->mutableCommandList.base = this;
    }

    void *asMutable() override {
        return static_cast<L0::MCL::MutableCommandList *>(&this->mutableCommandList);
    }
};

} // namespace ult
} // namespace L0
