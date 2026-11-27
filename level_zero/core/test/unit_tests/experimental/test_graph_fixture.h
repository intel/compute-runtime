/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "level_zero/core/test/unit_tests/experimental/test_graph.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0::ult {

struct GraphFixture : public DeviceFixture {
    void setUp() {
        debugManager.flags.ForceGraphImplicitSyncMutation.set(0);
        DeviceFixture::setUp();
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &cmdQueueDesc, &immCmdListHandle));
        immCmdList = L0::CommandList::fromHandle(immCmdListHandle);
    }
    void tearDown() {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(immCmdListHandle));
        DeviceFixture::tearDown();
    }

    auto setUint32ValueArguments(Mock<KernelImp> &kernel, uint32_t numArguments) -> void {
        kernel.checkPassedArgumentValues = true;
        kernel.passedArgumentValues.resize(numArguments);
        auto &explicitArgs = kernel.descriptor.payloadMappings.explicitArgs;
        explicitArgs.resize(numArguments);
        for (auto &explicitArg : explicitArgs) {
            explicitArg = NEO::ArgDescriptor{NEO::ArgDescriptor::argTValue};
            explicitArg.getTraits().argByValSize = sizeof(uint32_t);
            explicitArg.as<NEO::ArgDescValue>().elements.push_back({.size = sizeof(uint32_t), .sourceOffset = 0U});
        }
    }

    ze_command_list_handle_t immCmdListHandle;
    L0::CommandList *immCmdList = nullptr;
    DebugManagerStateRestore graphDbgRestorer;
};

using GraphTestApiCaptureWithDevice = Test<GraphFixture>;
using GraphTestInstantiationTest = Test<GraphFixture>;
using GraphExecution = Test<GraphFixture>;
using GraphTestApiCaptureBeginEnd = Test<GraphFixture>;

struct ExposedExecutableGraph : ExecutableGraph {
    using ExecutableGraph::trailingEventsPool;
};

struct DriverBackedGraphContextReturningSpecificCmdList : Mock<Context> {
    DriverBackedGraphContextReturningSpecificCmdList(DriverHandle *driverHandle) : Mock<Context>(driverHandle), driverHandleToReturn(driverHandle) {}

    DriverHandle *getDriverHandle() override {
        return driverHandleToReturn;
    }

    ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList, uint32_t estimatedNumberOfCommands) override {
        UNRECOVERABLE_IF(cmdListsToReturn.empty());
        *commandList = cmdListsToReturn.front();
        cmdListsToReturn.erase(cmdListsToReturn.begin());
        return ZE_RESULT_SUCCESS;
    }

    ~DriverBackedGraphContextReturningSpecificCmdList() override {
        for (auto &cmdList : cmdListsToReturn) {
            delete static_cast<L0::CommandList *>(cmdList);
        }
    }

    DriverHandle *driverHandleToReturn = nullptr;
    std::vector<Mock<CommandList> *> cmdListsToReturn;
};

} // namespace L0::ult
