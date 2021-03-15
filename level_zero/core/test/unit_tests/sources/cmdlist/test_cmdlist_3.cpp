/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

using Platforms = IsAtLeastProduct<IGFX_SKYLAKE>;

struct MemoryManagerCommandListCreateNegativeTest : public NEO::MockMemoryManager {
    MemoryManagerCommandListCreateNegativeTest(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MockMemoryManager(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithProperties(const NEO::AllocationProperties &properties) override {
        if (forceFailureInPrimaryAllocation) {
            return nullptr;
        }
        return NEO::MemoryManager::allocateGraphicsMemoryWithProperties(properties);
    }
    bool forceFailureInPrimaryAllocation = false;
};

struct CommandListCreateNegativeTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        memoryManager = new MemoryManagerCommandListCreateNegativeTest(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        for (uint32_t i = 0; i < numRootDevices; i++) {
            neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, i);
            devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        }

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
    MemoryManagerCommandListCreateNegativeTest *memoryManager = nullptr;
    const uint32_t numRootDevices = 1u;
};

TEST_F(CommandListCreateNegativeTest, whenDeviceAllocationFailsDuringCommandListCreateThenAppropriateValueIsReturned) {
    ze_result_t returnValue;
    memoryManager->forceFailureInPrimaryAllocation = true;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, returnValue);
    ASSERT_EQ(nullptr, commandList);
}

TEST_F(CommandListCreateNegativeTest, whenDeviceAllocationFailsDuringCommandListImmediateCreateThenAppropriateValueIsReturned) {
    ze_result_t returnValue;
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;
    memoryManager->forceFailureInPrimaryAllocation = true;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily,
                                                                              device,
                                                                              &desc,
                                                                              internalEngine,
                                                                              NEO::EngineGroupType::RenderCompute,
                                                                              returnValue));
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, returnValue);
    ASSERT_EQ(nullptr, commandList);
}

using CommandListCreate = Test<DeviceFixture>;

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenGettingAllocInRangeThenAllocFromMapReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = ptrOffset(cpuPtr, 0x10);
    auto newBufferSize = allocSize - 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_NE(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenSizeIsOutOfRangeThenNullPtrReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = ptrOffset(cpuPtr, 0x10);
    auto newBufferSize = allocSize + 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenPtrIsOutOfRangeThenNullPtrReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = reinterpret_cast<const void *>(gpuAddress - 0x100);
    auto newBufferSize = allocSize - 0x200;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenGetHostPtrAllocCalledThenCorrectOffsetIsSet, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));
    size_t expectedOffset = 0x10;
    auto newBufferPtr = ptrOffset(cpuPtr, expectedOffset);
    auto newBufferSize = allocSize - 0x20;
    auto newAlloc = commandList->getHostPtrAlloc(newBufferPtr, newBufferSize);
    EXPECT_NE(nullptr, newAlloc);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenPtrIsInMapThenAllocationReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = cpuPtr;
    auto newBufferSize = allocSize - 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, &alloc);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenPtrIsInMapButWithBiggerSizeThenNullPtrReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = cpuPtr;
    auto newBufferSize = allocSize + 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate, givenHostAllocInMapWhenPtrLowerThanAnyInMapThenNullPtrReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    NEO::MockGraphicsAllocation alloc(const_cast<void *>(cpuPtr), gpuAddress, allocSize);
    commandList->hostPtrMap.insert(std::make_pair(cpuPtr, &alloc));

    auto newBufferPtr = reinterpret_cast<const void *>(gpuAddress - 0x10);
    auto newBufferSize = allocSize - 0x20;
    auto newAlloc = commandList->getAllocationFromHostPtrMap(newBufferPtr, newBufferSize);
    EXPECT_EQ(newAlloc, nullptr);
    commandList->hostPtrMap.clear();
}

HWTEST2_F(CommandListCreate,
          givenCmdListHostPointerUsedWhenGettingAlignedAllocationThenRetrieveProperOffsetAndAddress,
          Platforms) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute);

    size_t cmdListHostPtrSize = MemoryConstants::pageSize;
    void *cmdListHostBuffer = device->getNEODevice()->getMemoryManager()->allocateSystemMemory(cmdListHostPtrSize, cmdListHostPtrSize);
    void *startMemory = cmdListHostBuffer;
    void *baseAddress = alignDown(startMemory, MemoryConstants::pageSize);
    size_t expectedOffset = ptrDiff(startMemory, baseAddress);

    AlignedAllocationData outData = commandList->getAlignedAllocation(device, startMemory, cmdListHostPtrSize);
    ASSERT_NE(nullptr, outData.alloc);
    auto firstAlloc = outData.alloc;
    auto expectedGpuAddress = static_cast<uintptr_t>(alignDown(outData.alloc->getGpuAddress(), MemoryConstants::pageSize));
    EXPECT_EQ(startMemory, outData.alloc->getUnderlyingBuffer());
    EXPECT_EQ(expectedGpuAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(expectedOffset, outData.offset);

    size_t offset = 0x20u;
    void *offsetMemory = ptrOffset(startMemory, offset);
    expectedOffset = ptrDiff(offsetMemory, baseAddress);
    EXPECT_EQ(outData.offset + offset, expectedOffset);

    outData = commandList->getAlignedAllocation(device, offsetMemory, 4u);
    ASSERT_NE(nullptr, outData.alloc);
    EXPECT_EQ(firstAlloc, outData.alloc);
    EXPECT_EQ(startMemory, outData.alloc->getUnderlyingBuffer());
    EXPECT_EQ(expectedGpuAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(expectedOffset, outData.offset);

    commandList->removeHostPtrAllocations();
    device->getNEODevice()->getMemoryManager()->freeSystemMemory(cmdListHostBuffer);
}

using PlatformSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_ICELAKE>;
HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyRegionHavingHostMemoryWithSignalAndWaitEventsUsingRenderEngineThenPipeControlIsFound, PlatformSupport) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, result));
    auto &commandContainer = commandList->commandContainer;

    void *src_buffer = reinterpret_cast<void *>(0x1234);
    void *dst_buffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    result = commandList->appendMemoryCopyRegion(dst_buffer, &dr, width, 0,
                                                 src_buffer, &sr, width, 0, events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyRegionHavingDeviceMemoryWithSignalAndWaitEventsUsingRenderEngineThenPipeControlIsNotFound, PlatformSupport) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, result));
    auto &commandContainer = commandList->commandContainer;

    void *src_buffer = nullptr;
    void *dst_buffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = device->getDriverHandle()->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &src_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    result = device->getDriverHandle()->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dst_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    result = commandList->appendMemoryCopyRegion(dst_buffer, &dr, width, 0,
                                                 src_buffer, &sr, width, 0, events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    device->getDriverHandle()->freeMem(src_buffer);
    device->getDriverHandle()->freeMem(dst_buffer);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryFillHavingDeviceMemoryWithSignalAndWaitEventsUsingRenderEngineThenPipeControlIsNotFound, PlatformSupport) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, result));
    auto &commandContainer = commandList->commandContainer;

    void *dst_buffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = device->getDriverHandle()->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dst_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    int one = 1;
    result = commandList->appendMemoryFill(dst_buffer, reinterpret_cast<void *>(&one), sizeof(one), 4096u,
                                           events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    device->getDriverHandle()->freeMem(dst_buffer);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryFillHavingSharedMemoryWithSignalAndWaitEventsUsingRenderEngineThenPipeControlIsFound, PlatformSupport) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, result));
    auto &commandContainer = commandList->commandContainer;

    void *dst_buffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = device->getDriverHandle()->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dst_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    int one = 1;
    result = commandList->appendMemoryFill(dst_buffer, reinterpret_cast<void *>(&one), sizeof(one), 4096u,
                                           events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    device->getDriverHandle()->freeMem(dst_buffer);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryFillHavingHostMemoryWithSignalAndWaitEventsUsingRenderEngineThenPipeControlIsFound, PlatformSupport) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, result));
    auto &commandContainer = commandList->commandContainer;

    void *dst_buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, 16384u, 4090u, &dst_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    int one = 1;
    result = commandList->appendMemoryFill(dst_buffer, reinterpret_cast<void *>(&one), sizeof(one), 4090u,
                                           events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    device->getDriverHandle()->freeMem(dst_buffer);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryFillHavingEventsWithDeviceScopeThenPCDueToWaitEventIsAddedAndPCDueToSignalEventIsAddedWithDCFlush, PlatformSupport) {
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, result));
    auto &commandContainer = commandList->commandContainer;

    void *dst_buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, 16384u, 4090u, &dst_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_DEVICE;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    int one = 1;
    result = commandList->appendMemoryFill(dst_buffer, reinterpret_cast<void *>(&one), sizeof(one), 4090u,
                                           events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<PIPE_CONTROL *>(*itor);
    EXPECT_TRUE(cmd->getDcFlushEnable());

    device->getDriverHandle()->freeMem(dst_buffer);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryFillHavingEventsWithDeviceScopeThenPCDueToWaitEventIsNotAddedAndPCDueToSignalEventIsAddedWithOutDCFlush, PlatformSupport) {
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, result));
    auto &commandContainer = commandList->commandContainer;

    void *dst_buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, 16384u, 4090u, &dst_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    int one = 1;
    result = commandList->appendMemoryFill(dst_buffer, reinterpret_cast<void *>(&one), sizeof(one), 4090u,
                                           events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<PIPE_CONTROL *>(*itor);
    EXPECT_FALSE(cmd->getDcFlushEnable());

    device->getDriverHandle()->freeMem(dst_buffer);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyRegionWithSignalAndWaitEventsUsingCopyEngineThenSuccessIsReturned, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, result));

    void *src_buffer = reinterpret_cast<void *>(0x1234);
    void *dst_buffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    result = commandList->appendMemoryCopyRegion(dst_buffer, &dr, width, 0,
                                                 src_buffer, &sr, width, 0, events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyRegionWithSignalAndInvalidWaitHandleUsingCopyEngineThenErrorIsReturned, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, result));

    void *src_buffer = reinterpret_cast<void *>(0x1234);
    void *dst_buffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    result = commandList->appendMemoryCopyRegion(dst_buffer, &dr, width, 0,
                                                 src_buffer, &sr, width, 0, events[0], 1u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST2_F(CommandListCreate, givenImmediateCommandListWhenMemoryCopyRegionWithSignalAndWaitEventsUsingRenderEngineThenSuccessIsReturned, Platforms) {
    Mock<CommandQueue> cmdQueue;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;

    void *src_buffer = reinterpret_cast<void *>(0x1234);
    void *dst_buffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    EXPECT_CALL(cmdQueue, executeCommandLists).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));
    EXPECT_CALL(cmdQueue, synchronize).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));

    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    result = commandList->appendMemoryCopyRegion(dst_buffer, &dr, width, 0,
                                                 src_buffer, &sr, width, 0, events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    commandList->cmdQImmediate = nullptr;
}

HWTEST2_F(CommandListCreate, givenImmediateCommandListWhenMemoryCopyRegionWithSignalAndWaitEventsUsingCopyEngineThenSuccessIsReturned, Platforms) {
    Mock<CommandQueue> cmdQueue;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::Copy);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;

    void *src_buffer = reinterpret_cast<void *>(0x1234);
    void *dst_buffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    EXPECT_CALL(cmdQueue, executeCommandLists).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));
    EXPECT_CALL(cmdQueue, synchronize).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));

    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    result = commandList->appendMemoryCopyRegion(dst_buffer, &dr, width, 0,
                                                 src_buffer, &sr, width, 0, events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    commandList->cmdQImmediate = nullptr;
}

HWTEST2_F(CommandListCreate, givenImmediateCommandListWhenMemoryCopyRegionWithSignalAndInvalidWaitHandleUsingCopyEngineThenErrorIsReturned, Platforms) {
    Mock<CommandQueue> cmdQueue;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::Copy);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;

    void *src_buffer = reinterpret_cast<void *>(0x1234);
    void *dst_buffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    result = commandList->appendMemoryCopyRegion(dst_buffer, &dr, width, 0,
                                                 src_buffer, &sr, width, 0, events[0], 1u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    commandList->cmdQImmediate = nullptr;
}

using HostPointerManagerCommandListTest = Test<HostPointerManagerFixure>;
HWTEST2_F(HostPointerManagerCommandListTest,
          givenImportedHostPointerWhenAppendMemoryFillUsingHostPointerThenAppendFillUsingHostPointerAllocation,
          Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute);

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    int pattern = 1;
    ret = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&pattern), sizeof(pattern), 64u, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest,
          givenImportedHostPointerAndCopyEngineWhenAppendMemoryFillUsingHostPointerThenAppendFillUsingHostPointerAllocation,
          Platforms) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    int pattern = 1;
    ret = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&pattern), sizeof(pattern), 64u, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest,
          givenHostPointerImportedWhenGettingAlignedAllocationThenRetrieveProperOffsetAndAddress,
          Platforms) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute);

    size_t pointerSize = MemoryConstants::pageSize;
    size_t offset = 100;
    void *offsetPointer = ptrOffset(heapPointer, offset);

    auto ret = hostDriverHandle->importExternalPointer(offsetPointer, offset);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto hostAllocation = hostDriverHandle->findHostPointerAllocation(heapPointer, pointerSize, device->getRootDeviceIndex());
    ASSERT_NE(nullptr, hostAllocation);

    AlignedAllocationData outData = commandList->getAlignedAllocation(device, heapPointer, pointerSize);
    auto expectedAlignedAddress = static_cast<uintptr_t>(hostAllocation->getGpuAddress());
    EXPECT_EQ(heapPointer, hostAllocation->getUnderlyingBuffer());
    EXPECT_EQ(expectedAlignedAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(hostAllocation, outData.alloc);
    EXPECT_EQ(0u, outData.offset);

    outData = commandList->getAlignedAllocation(device, offsetPointer, 2u);
    expectedAlignedAddress = static_cast<uintptr_t>(hostAllocation->getGpuAddress());
    EXPECT_EQ(heapPointer, hostAllocation->getUnderlyingBuffer());
    EXPECT_EQ(expectedAlignedAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(hostAllocation, outData.alloc);
    EXPECT_EQ(offset, outData.offset);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(HostPointerManagerCommandListTest, givenCommandListWhenMemoryFillWithSignalAndWaitEventsUsingRenderEngineThenPipeControlIsFound, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute);
    auto &commandContainer = commandList->commandContainer;

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                           events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

using SupportedPlatforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_ICELAKE>;
HWTEST2_F(HostPointerManagerCommandListTest, givenCommandListWhenMemoryFillWithSignalAndInvalidWaitHandleUsingRenderEngineThenErrorIsReturnedAndPipeControlIsNotAdded, SupportedPlatforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute);
    auto &commandContainer = commandList->commandContainer;

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                           events[0], 1u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(HostPointerManagerCommandListTest, givenCommandListWhenMemoryFillWithSignalAndWaitEventsUsingCopyEngineThenSuccessIsReturned, Platforms) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                           events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

using SupportedPlatforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_ICELAKE>;
HWTEST2_F(HostPointerManagerCommandListTest, givenCommandListWhenMemoryFillWithSignalAndiInvalidWaitHandleUsingCopyEngineThenErrorIsReturned, SupportedPlatforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy);
    auto &commandContainer = commandList->commandContainer;

    auto ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                           events[0], 1u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(HostPointerManagerCommandListTest, givenImmediateCommandListWhenMemoryFillWithSignalAndWaitEventsUsingRenderEngineThenSuccessIsReturned, Platforms) {
    Mock<CommandQueue> cmdQueue;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;

    ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    EXPECT_CALL(cmdQueue, executeCommandLists).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));
    EXPECT_CALL(cmdQueue, synchronize).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));

    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                           events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    commandList->cmdQImmediate = nullptr;
}

HWTEST2_F(HostPointerManagerCommandListTest, givenImmediateCommandListWhenMemoryFillWithSignalAndWaitEventsUsingCopyEngineThenSuccessIsReturned, Platforms) {
    Mock<CommandQueue> cmdQueue;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::Copy);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;

    ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    EXPECT_CALL(cmdQueue, executeCommandLists).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));
    EXPECT_CALL(cmdQueue, synchronize).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));

    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                           events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    commandList->cmdQImmediate = nullptr;
}

using SupportedPlatforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_ICELAKE>;
HWTEST2_F(HostPointerManagerCommandListTest, givenImmediateCommandListWhenMemoryFillWithSignalAndInvalidWaitHandleUsingCopyEngineThenErrorIsReturned, SupportedPlatforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    Mock<CommandQueue> cmdQueue;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t ret = commandList->initialize(device, NEO::EngineGroupType::Copy);
    auto &commandContainer = commandList->commandContainer;
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;

    ret = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    int one = 1;
    size_t size = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(hostDriverHandle.get(), 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    result = commandList->appendMemoryFill(heapPointer, reinterpret_cast<void *>(&one), sizeof(one), size,
                                           events[0], 1u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    ret = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    commandList->cmdQImmediate = nullptr;
}

} // namespace ult
} // namespace L0
