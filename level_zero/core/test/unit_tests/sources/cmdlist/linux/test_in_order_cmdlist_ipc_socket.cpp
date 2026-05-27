/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ipc_socket_server.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/internal/l0_event.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/driver_experimental/zex_api.h"

#include <map>
#include <vector>

namespace L0 {
namespace ult {

class MockIpcSocketServerForInOrderTests : public NEO::IpcSocketServer {
  public:
    bool initialize() override {
        running = true;
        return true;
    }
    void shutdown() override { running = false; }
    bool isRunning() const override { return running; }
    std::string getSocketPath() const override { return "mock_socket_path"; }
    bool registerHandle(uint64_t, int) override { return true; }
    bool unregisterHandle(uint64_t) override { return true; }
    bool running = false;
};

struct InOrderIpcSocketTests : public InOrderCmdListFixture {
    void SetUp() override {
        InOrderCmdListFixture::SetUp();

        static_cast<NEO::MockMemoryManager *>(device->getDriverHandle()->getMemoryManager())->storeIpcAllocations = true;
        setOpaqueHandleSupport(true);

        auto mockServer = new MockIpcSocketServerForInOrderTests();
        mockServer->running = true;
        device->getDriverHandle()->ipcSocketServer.reset(mockServer);
    }

    void assignInternalHandle(GraphicsAllocation *allocation) {
        auto memoryAllocation = static_cast<MemoryAllocation *>(allocation);
        if (memoryAllocation && memoryAllocation->internalHandle == 0) {
            memoryAllocation->internalHandle = ++internalHandleCounter;
        }
    }

    void enableEventSharing(InOrderFixtureMockEvent &event) {
        event.isSharableCounterBased = true;

        auto &inOrderEventHelper = static_cast<WhiteboxInOrderExecEventHelper &>(event.inOrderExecHelper);
        auto &sharableEventDataHelper = static_cast<WhiteboxSharableEventDataHelper &>(inOrderEventHelper.sharableEventDataHelper);

        if (!sharableEventDataHelper.allocation && sharableEventDataHelper.eventDataPtr) {
            auto savedData = *sharableEventDataHelper.eventDataPtr;
            sharableEventDataHelper.localTempStorage.reset();
            sharableEventDataHelper.eventDataPtr = nullptr;

            auto node = device->getInOrderSharableEventDataAllocator()->getTag();
            node->initialize();
            sharableEventDataHelper.initializeFromTagNode(*node, device->getRootDeviceIndex());
            *sharableEventDataHelper.eventDataPtr = savedData;
        }

        if (event.getInOrderExecEventHelper().isDataAssigned()) {
            assignInternalHandle(event.getInOrderExecEventHelper().getDeviceCounterAllocation());
            assignInternalHandle(event.getInOrderExecEventHelper().getHostCounterAllocation());
            assignInternalHandle(sharableEventDataHelper.allocation);
        }
    }

    void setOpaqueHandleSupport(bool enable) {
        context->settings.useOpaqueHandle = enable ? (OpaqueHandlingType::nthandle | OpaqueHandlingType::pidfd) : OpaqueHandlingType::none;

        auto defaultContext = Context::fromHandle(Context::fromHandle(device->getDriverHandle()->getDefaultContext()));
        defaultContext->settings.useOpaqueHandle = context->settings.useOpaqueHandle;
    }

    uint64_t internalHandleCounter = 0;
};

HWTEST_F(InOrderIpcSocketTests, givenSocketHandleSharingSupportedWhenGettingCounterBasedIpcHandleThenHandlesAreTracked) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto defaultContext = Context::fromHandle(device->getDriverHandle()->getDefaultContext());
    defaultContext->settings.useOpaqueHandle = OpaqueHandlingType::sockets;
    defaultContext->settings.handleType = IpcHandleType::fdHandle;
    EXPECT_TRUE(defaultContext->isSocketHandleSharingSupported());

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    EXPECT_FALSE(events[0]->exportedIpcServerHandles.empty());
}

HWTEST_F(InOrderIpcSocketTests, givenEventWithTrackedIpcHandlesWhenDestroyCalledThenHandlesAreUnregistered) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto defaultContext = Context::fromHandle(device->getDriverHandle()->getDefaultContext());
    defaultContext->settings.useOpaqueHandle = OpaqueHandlingType::sockets;
    defaultContext->settings.handleType = IpcHandleType::fdHandle;

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    EXPECT_FALSE(events[0]->exportedIpcServerHandles.empty());

    events[0].release()->destroy();
}

HWTEST_F(InOrderIpcSocketTests, givenEventWithTrackedIpcHandlesWhenUpdateInOrderExecStateWithDifferentAddressThenOldHandlesAreCleared) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(2, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto defaultContext = Context::fromHandle(device->getDriverHandle()->getDefaultContext());
    defaultContext->settings.useOpaqueHandle = OpaqueHandlingType::sockets;
    defaultContext->settings.handleType = IpcHandleType::fdHandle;

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    auto handleCountAfterFullExport = events[0]->exportedIpcServerHandles.size();
    EXPECT_GT(handleCountAfterFullExport, 0u);

    auto immCmdList2 = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    auto newInOrderExecInfo = immCmdList2->inOrderExecInfo;
    EXPECT_NE(newInOrderExecInfo->getBaseDeviceAddress(), events[0]->getInOrderExecEventHelper().getBaseDeviceAddress());

    events[0]->updateInOrderExecState(newInOrderExecInfo, 1, 0);

    EXPECT_LT(events[0]->exportedIpcServerHandles.size(), handleCountAfterFullExport);
}

HWTEST_F(InOrderIpcSocketTests, givenSocketSharingWhenGetIpcHandleCalledThenHandlesAreTracked) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);
    enableEventSharing(*events[0]);

    auto defaultContext = Context::fromHandle(device->getDriverHandle()->getDefaultContext());
    defaultContext->settings.useOpaqueHandle = OpaqueHandlingType::sockets;
    defaultContext->settings.handleType = IpcHandleType::fdHandle;
    EXPECT_TRUE(defaultContext->isSocketHandleSharingSupported());

    EXPECT_TRUE(events[0]->exportedIpcServerHandles.empty());

    ze_ipc_event_counter_based_handle_t zeIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedGetIpcHandle(events[0]->toHandle(), &zeIpcData));

    EXPECT_FALSE(events[0]->exportedIpcServerHandles.empty());
}

} // namespace ult
} // namespace L0
