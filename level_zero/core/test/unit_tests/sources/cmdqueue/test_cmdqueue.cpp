/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "test.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"

namespace L0 {
namespace ult {

using CommandQueueCreate = Test<DeviceFixture>;

TEST_F(CommandQueueCreate, whenCreatingCommandQueueThenItIsInitialized) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_NONE,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};

    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());

    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          false);
    ASSERT_NE(nullptr, commandQueue);

    L0::CommandQueueImp *commandQueueImp = reinterpret_cast<L0::CommandQueueImp *>(commandQueue);

    EXPECT_EQ(csr.get(), commandQueueImp->getCsr());
    EXPECT_EQ(device, commandQueueImp->getDevice());
    EXPECT_EQ(0u, commandQueueImp->getTaskCount());

    commandQueue->destroy();
}

using CommandQueueSBASupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

struct MockMemoryManagerCommandQueueSBA : public MemoryManagerMock {
    MockMemoryManagerCommandQueueSBA(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
    MOCK_METHOD1(getInternalHeapBaseAddress, uint64_t(uint32_t rootDeviceIndex));
};

struct CommandQueueProgramSBATest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        memoryManager = new ::testing::NiceMock<MockMemoryManagerCommandQueueSBA>(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, rootDeviceIndex);
        std::vector<std::unique_ptr<NEO::Device>> devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        device = driverHandle->devices[0];
    }
    void TearDown() override {
    }

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    MockMemoryManagerCommandQueueSBA *memoryManager = nullptr;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};

HWTEST2_F(CommandQueueProgramSBATest, whenCreatingCommandQueueThenItIsInitialized, CommandQueueSBASupport) {
    ze_command_queue_desc_t desc = {};
    desc.version = ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT;
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false);

    uint32_t alignedSize = 4096u;
    NEO::LinearStream child(commandQueue->commandStream->getSpace(alignedSize), alignedSize);

    EXPECT_CALL(*memoryManager, getInternalHeapBaseAddress(rootDeviceIndex))
        .Times(1);

    commandQueue->programGeneralStateBaseAddress(0u, child);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenCmdQueueWithBlitCopyWhenExecutingNonCopyBlitCommandListThenWrongCommandListStatusReturned) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_COPY_ONLY,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};

    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());

    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          true);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(status, ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenCmdQueueWithBlitCopyWhenExecutingCopyBlitCommandListThenSuccessReturned) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_COPY_ONLY,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};

    auto defaultCsr = neoDevice->getDefaultEngine().commandStreamReceiver;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          defaultCsr,
                                                          &desc,
                                                          true);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));
    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(status, ZE_RESULT_SUCCESS);

    commandQueue->destroy();
}

using CommandQueueDestroySupport = IsAtLeastProduct<IGFX_SKYLAKE>;
using CommandQueueDestroy = Test<DeviceFixture>;

HWTEST2_F(CommandQueueDestroy, whenCommandQueueDestroyIsCalledPrintPrintfOutputIsCalled, CommandQueueDestroySupport) {
    ze_command_queue_desc_t desc = {};
    desc.version = ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT;
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false);

    Mock<Kernel> kernel;
    commandQueue->printfFunctionContainer.push_back(&kernel);

    EXPECT_EQ(0u, kernel.printPrintfOutputCalledTimes);
    commandQueue->destroy();
    EXPECT_EQ(1u, kernel.printPrintfOutputCalledTimes);
}

} // namespace ult
} // namespace L0