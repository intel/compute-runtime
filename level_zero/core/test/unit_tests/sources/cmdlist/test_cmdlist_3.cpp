/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
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
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
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
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
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
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
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
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
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
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
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
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
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
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
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
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

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

    size_t offset = 0x21u;
    void *offsetMemory = ptrOffset(startMemory, offset);
    expectedOffset = ptrDiff(offsetMemory, baseAddress);
    EXPECT_EQ(outData.offset + offset, expectedOffset);

    outData = commandList->getAlignedAllocation(device, offsetMemory, 4u);
    ASSERT_NE(nullptr, outData.alloc);
    EXPECT_EQ(firstAlloc, outData.alloc);
    EXPECT_EQ(startMemory, outData.alloc->getUnderlyingBuffer());
    EXPECT_EQ(expectedGpuAddress, outData.alignedAllocationPtr);
    EXPECT_EQ((expectedOffset & (EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment() - 1)), outData.offset);

    commandList->removeHostPtrAllocations();
    device->getNEODevice()->getMemoryManager()->freeSystemMemory(cmdListHostBuffer);
}

using PlatformSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_ICELAKE>;
HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyRegionHavingHostMemoryWithSignalAndWaitEventsUsingRenderEngineThenPipeControlIsFound, PlatformSupport) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    auto &commandContainer = commandList->commandContainer;

    void *src_buffer = reinterpret_cast<void *>(0x1234);
    void *dst_buffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    auto &commandContainer = commandList->commandContainer;

    void *src_buffer = nullptr;
    void *dst_buffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &src_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dst_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
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

    context->freeMem(src_buffer);
    context->freeMem(dst_buffer);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryFillHavingDeviceMemoryWithSignalAndWaitEventsUsingRenderEngineThenPipeControlIsNotFound, PlatformSupport) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    auto &commandContainer = commandList->commandContainer;

    void *dst_buffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dst_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
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

    context->freeMem(dst_buffer);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryFillHavingSharedMemoryWithSignalAndWaitEventsUsingRenderEngineThenPipeControlIsFound, PlatformSupport) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    auto &commandContainer = commandList->commandContainer;

    void *dst_buffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dst_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
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

    context->freeMem(dst_buffer);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryFillHavingHostMemoryWithSignalAndWaitEventsUsingRenderEngineThenPipeControlIsFound, PlatformSupport) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    auto &commandContainer = commandList->commandContainer;

    void *dst_buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, 16384u, 4090u, &dst_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
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

    context->freeMem(dst_buffer);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryFillHavingEventsWithDeviceScopeThenPCDueToWaitEventIsAddedAndPCDueToSignalEventIsAddedWithDCFlush, PlatformSupport) {
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    auto &commandContainer = commandList->commandContainer;

    void *dst_buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, 16384u, 4090u, &dst_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_DEVICE;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
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

    context->freeMem(dst_buffer);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryFillHavingEventsWithDeviceScopeThenPCDueToWaitEventIsNotAddedAndPCDueToSignalEventIsAddedWithOutDCFlush, PlatformSupport) {
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));
    auto &commandContainer = commandList->commandContainer;

    void *dst_buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, 16384u, 4090u, &dst_buffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
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

    context->freeMem(dst_buffer);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenMemoryCopyRegionWithSignalAndWaitEventsUsingCopyEngineThenSuccessIsReturned, Platforms) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, result));

    void *src_buffer = reinterpret_cast<void *>(0x1234);
    void *dst_buffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, result));

    void *src_buffer = reinterpret_cast<void *>(0x1234);
    void *dst_buffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    result = commandList->appendMemoryCopyRegion(dst_buffer, &dr, width, 0,
                                                 src_buffer, &sr, width, 0, events[0], 1u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST2_F(CommandListCreate, givenImmediateCommandListWhenMemoryCopyRegionWithSignalAndWaitEventsUsingRenderEngineThenSuccessIsReturned, Platforms) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    void *src_buffer = reinterpret_cast<void *>(0x1234);
    void *dst_buffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    result = commandList0->appendMemoryCopyRegion(dst_buffer, &dr, width, 0,
                                                  src_buffer, &sr, width, 0, events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(CommandListCreate, givenImmediateCommandListWhenMemoryCopyRegionWithSignalAndWaitEventsUsingRenderEngineInALoopThenSuccessIsReturned) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t ret = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               ret));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    void *src_buffer = reinterpret_cast<void *>(0x1234);
    void *dst_buffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};

    for (auto i = 0; i < 2000; i++) {
        ret = commandList0->appendMemoryCopyRegion(dst_buffer, &dr, width, 0,
                                                   src_buffer, &sr, width, 0, events[0], 1u, &events[1]);
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST2_F(CommandListCreate, givenImmediateCommandListWhenMemoryCopyRegionWithSignalAndWaitEventsUsingCopyEngineThenSuccessIsReturned, Platforms) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    void *src_buffer = reinterpret_cast<void *>(0x1234);
    void *dst_buffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    auto result = commandList0->appendMemoryCopyRegion(dst_buffer, &dr, width, 0,
                                                       src_buffer, &sr, width, 0, events[0], 1u, &events[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using ImageSupported = IsAtLeastProduct<IGFX_SKYLAKE>;

HWTEST2_F(CommandListCreate, givenImmediateCommandListWhenCopyRegionFromImageToImageUsingRenderThenSuccessIsReturned, ImageSupported) {
    const ze_command_queue_desc_t queueDesc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &queueDesc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &desc);
    imageHWDst->initialize(device, &desc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    returnValue = commandList0->appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), &dstRegion, &srcRegion, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

HWTEST2_F(CommandListCreate, givenImmediateCommandListWhenCopyRegionFromImageToImageUsingCopyWintInvalidRegionArguementsThenErrorIsReturned, ImageSupported) {
    const ze_command_queue_desc_t queueDesc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &queueDesc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &desc);
    imageHWDst->initialize(device, &desc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_image_region_t dstRegion = {2, 2, 2, 4, 4, 4};
    returnValue = commandList0->appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), &dstRegion, &srcRegion, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
}

HWTEST2_F(CommandListCreate, givenImmediateCommandListWhenCopyFromImageToImageUsingRenderThenSuccessIsReturned, ImageSupported) {
    const ze_command_queue_desc_t queueDesc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &queueDesc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &desc);
    imageHWDst->initialize(device, &desc);

    returnValue = commandList0->appendImageCopy(imageHWDst->toHandle(), imageHWSrc->toHandle(), nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

HWTEST2_F(CommandListCreate, givenImmediateCommandListWhenMemoryCopyRegionWithSignalAndInvalidWaitHandleUsingCopyEngineThenErrorIsReturned, Platforms) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::Copy,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    void *src_buffer = reinterpret_cast<void *>(0x1234);
    void *dst_buffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 2;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));

    std::vector<ze_event_handle_t> events;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event.get());
    eventDesc.index = 1;
    auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    events.push_back(event1.get());

    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    result = commandList0->appendMemoryCopyRegion(dst_buffer, &dr, width, 0,
                                                  src_buffer, &sr, width, 0, events[0], 1u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithASyncModeAndAppendSignalEventWithTimestampThenUpdateTaskCountNeededFlagIsDisabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event_object->csr);

    commandList->appendSignalEvent(event);

    auto result = event_object->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event_object->queryStatus(), ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithASyncModeAndAppendBarrierThenUpdateTaskCountNeededFlagIsDisabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event_object->csr);

    commandList->appendBarrier(event, 0, nullptr);

    auto result = event_object->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event_object->queryStatus(), ZE_RESULT_SUCCESS);

    commandList->appendBarrier(nullptr, 0, nullptr);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithASyncModeAndAppendEventResetThenUpdateTaskCountNeededFlagIsDisabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event_object->csr);

    commandList->appendEventReset(event);

    auto result = event_object->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event_object->queryStatus(), ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenQueueDescriptionwhenCreatingImmediateCommandListForCopyEnigneThenItHasImmediateCommandQueueCreated) {
    auto &engines = neoDevice->getEngineGroups();
    uint32_t numaAvailableEngineGroups = 0;
    for (uint32_t ordinal = 0; ordinal < static_cast<uint32_t>(NEO::EngineGroupType::MaxEngineGroups); ordinal++) {
        if (engines[ordinal].size()) {
            numaAvailableEngineGroups++;
        }
    }
    for (uint32_t ordinal = 0; ordinal < numaAvailableEngineGroups; ordinal++) {
        uint32_t engineGroupIndex = ordinal;
        device->mapOrdinalForAvailableEngineGroup(&engineGroupIndex);
        for (uint32_t index = 0; index < engines[engineGroupIndex].size(); index++) {
            ze_command_queue_desc_t desc = {};
            desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
            desc.ordinal = ordinal;
            desc.index = index;
            ze_result_t returnValue;
            std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::Copy, returnValue));
            ASSERT_NE(nullptr, commandList);

            EXPECT_EQ(device, commandList->device);
            EXPECT_EQ(CommandList::CommandListType::TYPE_IMMEDIATE, commandList->cmdListType);
            EXPECT_NE(nullptr, commandList->cmdQImmediate);

            ze_event_pool_desc_t eventPoolDesc = {};
            eventPoolDesc.count = 3;
            eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

            ze_event_desc_t eventDesc = {};
            eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
            eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
            auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
            auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
            auto event1 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
            auto event2 = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
            ze_event_handle_t events[] = {event1->toHandle(), event2->toHandle()};

            commandList->appendBarrier(nullptr, 0, nullptr);
            commandList->appendBarrier(event->toHandle(), 2, events);

            auto result = event->hostSignal();
            ASSERT_EQ(ZE_RESULT_SUCCESS, result);
            result = event1->hostSignal();
            ASSERT_EQ(ZE_RESULT_SUCCESS, result);
            result = event2->hostSignal();
            ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        }
    }
}

HWTEST2_F(CommandListCreate, whenGettingCommandsToPatchThenCorrectValuesAreReturned, Platforms) {
    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    EXPECT_EQ(&commandList->requiredStreamState, &commandList->getRequiredStreamState());
    EXPECT_EQ(&commandList->finalStreamState, &commandList->getFinalStreamState());
    EXPECT_EQ(&commandList->commandsToPatch, &commandList->getCommandsToPatch());
}

HWTEST2_F(CommandListCreate, givenNonEmptyCommandsToPatchWhenClearCommandsToPatchIsCalledThenCommandsAreCorrectlyCleared, Platforms) {
    using VFE_STATE_TYPE = typename FamilyType::VFE_STATE_TYPE;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    EXPECT_TRUE(pCommandList->commandsToPatch.empty());
    EXPECT_NO_THROW(pCommandList->clearCommandsToPatch());
    EXPECT_TRUE(pCommandList->commandsToPatch.empty());

    CommandList::CommandToPatch commandToPatch{};
    pCommandList->commandsToPatch.push_back(commandToPatch);
    EXPECT_ANY_THROW(pCommandList->clearCommandsToPatch());
    pCommandList->commandsToPatch.clear();

    commandToPatch.type = CommandList::CommandToPatch::CommandType::FrontEndState;
    pCommandList->commandsToPatch.push_back(commandToPatch);
    EXPECT_ANY_THROW(pCommandList->clearCommandsToPatch());
    pCommandList->commandsToPatch.clear();

    commandToPatch.pCommand = new VFE_STATE_TYPE;
    pCommandList->commandsToPatch.push_back(commandToPatch);
    EXPECT_NO_THROW(pCommandList->clearCommandsToPatch());
    EXPECT_TRUE(pCommandList->commandsToPatch.empty());
}

} // namespace ult
} // namespace L0
