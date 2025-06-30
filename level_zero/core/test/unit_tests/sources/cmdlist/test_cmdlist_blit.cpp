/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForMemFill : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    using BaseClass = WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>;

    MockCommandListForMemFill() : BaseClass() {}

    using BaseClass::getAllocationOffsetForAppendBlitFill;

    AlignedAllocationData getAlignedAllocationData(L0::Device *device, bool sharedSystemEnabled, const void *buffer, uint64_t bufferSize, bool allowHostCopy, bool copyOffload) override {
        return {0, 0, nullptr, true};
    }
    ze_result_t appendMemoryCopyBlit(uintptr_t dstPtr,
                                     NEO::GraphicsAllocation *dstPtrAlloc,
                                     uint64_t dstOffset, uintptr_t srcPtr,
                                     NEO::GraphicsAllocation *srcPtrAlloc,
                                     uint64_t srcOffset,
                                     uint64_t size, Event *signalEvent) override {
        appendMemoryCopyBlitCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    uint32_t appendMemoryCopyBlitCalledTimes = 0;
};
class MockDriverHandle : public L0::DriverHandleImp {
  public:
    bool findAllocationDataForRange(const void *buffer,
                                    size_t size,
                                    NEO::SvmAllocationData *&allocData) override {
        mockAllocation.reset(new NEO::MockGraphicsAllocation(rootDeviceIndex, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount));
        data.gpuAllocations.addAllocation(mockAllocation.get());
        allocData = &data;
        return true;
    }
    const uint32_t rootDeviceIndex = 0u;
    std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
    NEO::SvmAllocationData data{rootDeviceIndex};
};

using AppendMemoryCopyTests = Test<AppendMemoryCopyFixture>;

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWhenAppenBlitFillCalledWithLargePatternSizeThenMemCopyWasCalled) {
    MockCommandListForMemFill<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    uint64_t pattern[4] = {1, 2, 3, 4};
    void *ptr = reinterpret_cast<void *>(0x1234);
    auto ret = cmdList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, ret);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWhenAppenBlitFillToNotDeviceMemThenInvalidArgumentReturned) {
    MockCommandListForMemFill<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    uint8_t pattern = 1;
    void *ptr = reinterpret_cast<void *>(0x1234);
    auto ret = cmdList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWhenAppenBlitFillToSharedSystemUsmThenSuccessReturned) {
    MockCommandListForMemFill<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.set(1);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};

    sharedSystemMemCapabilities = 0xf; // enables return true for Device::areSharedSystemAllocationsAllowed()

    uint8_t pattern = 1;
    size_t size = 0x1000;
    void *ptr = malloc(size); // reinterpret_cast<void *>(0x1234);
    auto ret = cmdList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ret, ZE_RESULT_SUCCESS);
    free(ptr);
}

using MemFillPlatforms = IsGen12LP;

HWTEST2_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWhenAppenBlitFillThenCopyBltIsProgrammed, MemFillPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    MockCommandListForMemFill<FamilyType::gfxCoreFamily> commandList;
    MockDriverHandle driverHandleMock;
    NEO::DeviceVector neoDevices;
    neoDevices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandleMock.initialize(std::move(neoDevices));
    device->setDriverHandle(&driverHandleMock);
    commandList.initialize(device, NEO::EngineGroupType::copy, 0u);
    uint16_t pattern = 1;
    void *ptr = reinterpret_cast<void *>(0x1234);
    commandList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr, 0, nullptr, copyParams);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList.getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    device->setDriverHandle(driverHandle.get());
}

HWTEST_F(AppendMemoryCopyTests, givenExternalHostPointerAllocationWhenPassedToAppendBlitFillThenProgramDestinationAddressCorrectly) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;

    L0::Device *device = driverHandle->devices[0];

    size_t size = 1024;
    auto hostPointer = std::make_unique<uint8_t[]>(size);
    auto ret = driverHandle->importExternalPointer(hostPointer.get(), MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto gpuAllocation = device->getDriverHandle()->findHostPointerAllocation(hostPointer.get(), size, 0);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(NEO::AllocationType::externalHostPtr, gpuAllocation->getAllocationType());

    MockCommandListForMemFill<FamilyType::gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::copy, 0u);

    uint32_t pattern = 1;
    ze_result_t result = commandList.appendMemoryFill(hostPointer.get(), reinterpret_cast<void *>(&pattern), sizeof(pattern), size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList.getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    auto cmd = genCmdCast<XY_COLOR_BLT *>(*itor);
    uint64_t offset = commandList.getAllocationOffsetForAppendBlitFill(hostPointer.get(), *gpuAllocation);
    EXPECT_EQ(cmd->getDestinationBaseAddress(), ptrOffset(gpuAllocation->getGpuAddress(), offset));

    ret = driverHandle->releaseImportedPointer(hostPointer.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListAndHostPointersWhenMemoryCopyCalledThenPipeControlWithDcFlushAddedIsNotAddedAfterBlitCopy) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams);

    auto &commandContainer = commandList->getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());

    EXPECT_EQ(genCmdList.end(), itor);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListAndHostPointersWhenMemoryCopyRegionCalledThenPipeControlWithDcFlushAddedIsNotAddedAfterBlitCopy) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);

    auto &commandContainer = commandList->getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());

    EXPECT_EQ(genCmdList.end(), itor);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListThenDcFlushIsNotAddedAfterBlitCopy) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    uintptr_t srcPtr = 0x5001;
    uintptr_t dstPtr = 0x7001;
    uint64_t srcOffset = 0x101;
    uint64_t dstOffset = 0x201;
    uint64_t copySize = 0x301;
    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(srcPtr), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(dstPtr), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    commandList->appendMemoryCopyBlit(ptrOffset(dstPtr, dstOffset), &mockAllocationDst, 0, ptrOffset(srcPtr, srcOffset), &mockAllocationSrc, 0, copySize, nullptr);

    auto &commandContainer = commandList->getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);
    auto cmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(cmd->getDestinationBaseAddress(), ptrOffset(dstPtr, dstOffset));
    EXPECT_EQ(cmd->getSourceBaseAddress(), ptrOffset(srcPtr, srcOffset));
}

HWTEST_F(AppendMemoryCopyTests, givenCopyCommandListWhenTimestampPassedToMemoryCopyRegionBlitThenTimeStampRegistersAreAdded) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    AlignedAllocationData srcAllocationData = {mockAllocationSrc.gpuAddress, 0, &mockAllocationSrc, false};
    AlignedAllocationData dstAllocationData = {mockAllocationDst.gpuAddress, 0, &mockAllocationDst, false};
    commandList->appendMemoryCopyBlitRegion(&srcAllocationData, &dstAllocationData, srcRegion, dstRegion, {0, 0, 0}, 0, 0, 0, 0, 0, 0, event.get(), 0, nullptr, false, false);
    GenCmdList cmdList;

    auto baseAddr = event->getGpuAddress(device);
    auto contextStartOffset = event->getContextStartOffset();
    auto globalStartOffset = event->getGlobalStartOffset();
    auto contextEndOffset = event->getContextEndOffset();
    auto globalEndOffset = event->getGlobalEndOffset();

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::globalTimestampLdw);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalStartOffset));
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextStartOffset));
    itor++;
    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::globalTimestampLdw);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalEndOffset));
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextEndOffset));
    itor++;
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyCommandListWhenTimestampPassedToImageCopyBlitThenTimeStampRegistersAreAdded) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    commandList->useAdditionalBlitProperties = false;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    CmdListMemoryCopyParams copyParams = {};
    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, event.get(), 0, nullptr, copyParams);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::globalTimestampLdw);
}

HWTEST2_F(AppendMemoryCopyTests, givenWaitWhenWhenAppendBlitCalledThenProgramSemaphore, MatchAny) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto cmdStream = commandList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    CmdListMemoryCopyParams copyParams = {};

    {
        commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, event.get(), 0, nullptr, copyParams);
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), itor);
    }

    {
        offset = cmdStream->getUsed();
        auto eventHandle = event->toHandle();

        commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, nullptr, 1, &eventHandle, copyParams);
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
        auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), itor);
    }
}

using ImageSupport = IsGen12LP;
HWTEST2_F(AppendMemoryCopyTests, givenCopyCommandListWhenCopyFromImagBlitThenCommandAddedToStream, ImageSupport) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), nullptr, nullptr, nullptr, 0, nullptr, copyParams);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

using AppendMemoryCopyFromContext = AppendMemoryCopyTests;

HWTEST_F(AppendMemoryCopyFromContext, givenCommandListThenUpOnPerformingAppendMemoryCopyFromContextSuccessIsReturned) {

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    auto result = commandList->appendMemoryCopyFromContext(dstPtr, nullptr, srcPtr, 8, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

struct IsAtLeastXeCoreAndNotXe2HpgCoreWith2DArrayImageSupport {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsAtLeastGfxCore<IGFX_XE_HP_CORE>::isMatched<productFamily>() && !IsXe2HpgCore::isMatched<productFamily>() && NEO::HwMapper<productFamily>::GfxProduct::supportsSampler;
    }
};
HWTEST2_F(AppendMemoryCopyTests, givenCopyCommandListWhenTiled1DArrayImagePassedToImageCopyBlitThenTransformedTo2DArrayCopy, IsAtLeastXeCoreAndNotXe2HpgCoreWith2DArrayImageSupport) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);

    auto gmmSrc = std::make_unique<MockGmm>(device->getNEODevice()->getGmmHelper());
    auto resourceInfoSrc = static_cast<MockGmmResourceInfo *>(gmmSrc->gmmResourceInfo.get());
    resourceInfoSrc->getResourceFlags()->Info.Tile64 = 1;
    resourceInfoSrc->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
    resourceInfoSrc->mockResourceCreateParams.ArraySize = 8;

    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    mockAllocationSrc.setGmm(gmmSrc.get(), 0);
    mockAllocationDst.setGmm(gmmSrc.get(), 0);

    size_t arrayLevels = 8;
    size_t depth = 1;
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 4, 4, 4, 4, 1, {1, arrayLevels, depth}, {1, arrayLevels, depth}, {1, arrayLevels, depth}, nullptr, 0, nullptr, copyParams);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<XY_BLOCK_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<XY_BLOCK_COPY_BLT *>(*itor);
    EXPECT_EQ(cmd->getSourceSurfaceDepth(), arrayLevels);
    EXPECT_EQ(cmd->getSourceSurfaceHeight(), depth);
}

HWTEST2_F(AppendMemoryCopyTests, givenCopyCommandListWhenNotTiled1DArrayImagePassedToImageCopyBlitThenNotTransformedTo2DArrayCopy, IsAtLeastXeCoreAndNotXe2HpgCoreWith2DArrayImageSupport) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);

    auto gmmSrc = std::make_unique<MockGmm>(device->getNEODevice()->getGmmHelper());
    auto resourceInfoSrc = static_cast<MockGmmResourceInfo *>(gmmSrc->gmmResourceInfo.get());
    resourceInfoSrc->getResourceFlags()->Info.Tile64 = 0;
    resourceInfoSrc->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
    resourceInfoSrc->mockResourceCreateParams.ArraySize = 8;

    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    mockAllocationSrc.setGmm(gmmSrc.get(), 0);
    mockAllocationDst.setGmm(gmmSrc.get(), 0);

    size_t arrayLevels = 8;
    size_t depth = 1;
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, arrayLevels, depth}, {1, arrayLevels, depth}, {1, arrayLevels, depth}, nullptr, 0, nullptr, copyParams);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<XY_BLOCK_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<XY_BLOCK_COPY_BLT *>(*itor);
    EXPECT_EQ(cmd->getSourceSurfaceDepth(), depth);
    EXPECT_EQ(cmd->getSourceSurfaceHeight(), arrayLevels);
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForAdditionalBlitProperties : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    using BaseClass = WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>;
    using BaseClass::setAdditionalBlitProperties;
    using BaseClass::useAdditionalBlitProperties;
};

HWTEST_F(AppendMemoryCopyTests, givenBlitPropertiesWhenCallingSetAdditionalBlitPropertiesThenSyncPropertiesExtRemainsUnchanged) {
    NEO::BlitProperties blitProperties{}, blitProperties2{}, blitPropertiesExpected{};
    EncodePostSyncArgs &postSyncArgs = blitProperties.blitSyncProperties.postSyncArgs;
    EncodePostSyncArgs &postSyncArgs2 = blitProperties2.blitSyncProperties.postSyncArgs;
    EncodePostSyncArgs &postSyncArgsExpected = blitPropertiesExpected.blitSyncProperties.postSyncArgs;

    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties<FamilyType::gfxCoreFamily>>();
    EXPECT_FALSE(commandList->useAdditionalBlitProperties);
    commandList->setAdditionalBlitProperties(blitProperties, nullptr, false);
    EXPECT_EQ(postSyncArgs.isTimestampEvent, postSyncArgsExpected.isTimestampEvent);
    EXPECT_EQ(postSyncArgs.postSyncImmValue, postSyncArgsExpected.postSyncImmValue);
    EXPECT_EQ(postSyncArgs.interruptEvent, postSyncArgsExpected.interruptEvent);
    EXPECT_EQ(postSyncArgs.eventAddress, postSyncArgsExpected.eventAddress);

    commandList->useAdditionalBlitProperties = true;
    commandList->setAdditionalBlitProperties(blitProperties2, nullptr, false);
    EXPECT_EQ(postSyncArgs2.isTimestampEvent, postSyncArgsExpected.isTimestampEvent);
    EXPECT_EQ(postSyncArgs2.postSyncImmValue, postSyncArgsExpected.postSyncImmValue);
    EXPECT_EQ(postSyncArgs2.interruptEvent, postSyncArgsExpected.interruptEvent);
    EXPECT_EQ(postSyncArgs2.eventAddress, postSyncArgsExpected.eventAddress);
    EXPECT_EQ(nullptr, postSyncArgs2.inOrderExecInfo);
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForAdditionalBlitProperties2 : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    using BaseClass = WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>;
    using BaseClass::useAdditionalBlitProperties;
    void setAdditionalBlitProperties(NEO::BlitProperties &blitProperties, Event *signalEvent, bool useAdditionalTimestamp) override {
        additionalBlitPropertiesCalled++;
        BaseClass::setAdditionalBlitProperties(blitProperties, signalEvent, useAdditionalTimestamp);
    }
    void appendSignalInOrderDependencyCounter(Event *signalEvent, bool copyOffloadOperation, bool stall, bool textureFlushRequired) override {
        appendSignalInOrderDependencyCounterCalled++;
        BaseClass::appendSignalInOrderDependencyCounter(signalEvent, copyOffloadOperation, stall, textureFlushRequired);
    }
    uint32_t additionalBlitPropertiesCalled = 0;
    uint32_t appendSignalInOrderDependencyCounterCalled = 0;
};

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendMemoryCopyBlitThenAdditionalBlitPropertiesCalled) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);

    ze_device_mem_alloc_desc_t deviceDesc = {};

    void *srcBuffer = nullptr;
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &srcBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, srcBuffer);

    void *dstBuffer = nullptr;
    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);

    commandList->useAdditionalBlitProperties = false;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryCopy(dstBuffer, srcBuffer, 4906u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    commandList->appendMemoryCopy(dstBuffer, srcBuffer, 4906u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(3u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyCopyBlt, commandList->inOrderPatchCmds[2].patchCmdType);

    context->freeMem(dstBuffer);
    context->freeMem(srcBuffer);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendMemoryCopyBlitThenInOrderPatchCmdsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0);

    ze_device_mem_alloc_desc_t deviceDesc = {};

    void *srcBuffer = nullptr;
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &srcBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, srcBuffer);

    void *dstBuffer = nullptr;
    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);

    commandList->useAdditionalBlitProperties = true;
    CmdListMemoryCopyParams copyParams = {};
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendMemoryCopy(dstBuffer, srcBuffer, 4906u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());

    context->freeMem(dstBuffer);
    context->freeMem(srcBuffer);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendMemoryCopyImageBlitThenAdditionalBlitPropertiesCalled) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);
    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    commandList->useAdditionalBlitProperties = false;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());

    commandList->useAdditionalBlitProperties = true;

    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(3u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyBlockCopyBlt, commandList->inOrderPatchCmds.back().patchCmdType);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendMemoryCopyImageBlitThenInOrderPatchCmdsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0);
    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendMemoryCopyRegionThenAdditionalBlitPropertiesCalled) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);
    void *srcBuffer = reinterpret_cast<void *>(0x1234);
    void *dstBuffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    CmdListMemoryCopyParams copyParams = {};

    commandList->useAdditionalBlitProperties = false;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendMemoryCopyRegion(dstBuffer, &dr, width, 0,
                                        srcBuffer, &sr, width, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());

    commandList->useAdditionalBlitProperties = true;
    commandList->appendMemoryCopyRegion(dstBuffer, &dr, width, 0,
                                        srcBuffer, &sr, width, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(3u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyCopyBlt, commandList->inOrderPatchCmds[2].patchCmdType);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendMemoryCopyRegionThenInOrderPatchCmdsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0);
    void *srcBuffer = reinterpret_cast<void *>(0x1234);
    void *dstBuffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    CmdListMemoryCopyParams copyParams = {};

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendMemoryCopyRegion(dstBuffer, &dr, width, 0,
                                        srcBuffer, &sr, width, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendBlitFillThenAdditionalBlitPropertiesCalled) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);

    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    uint32_t one = 1u;
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);
    CmdListMemoryCopyParams copyParams = {};

    commandList->useAdditionalBlitProperties = false;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint8_t), 4096u, nullptr, 0, nullptr, copyParams);

    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());

    commandList->useAdditionalBlitProperties = true;
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint8_t), 4096u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(3u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::memSet, commandList->inOrderPatchCmds[2].patchCmdType);
    context->freeMem(dstBuffer);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendBlitWithTwoBytesPatternFillThenAdditionalBlitPropertiesCalled) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);

    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    uint32_t one = 1u;
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);
    CmdListMemoryCopyParams copyParams = {};

    commandList->maxFillPaternSizeForCopyEngine = 4;

    commandList->useAdditionalBlitProperties = false;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint16_t), 4096u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());

    commandList->useAdditionalBlitProperties = true;
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint16_t), 4096u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(3u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyColorBlt, commandList->inOrderPatchCmds[2].patchCmdType);
    context->freeMem(dstBuffer);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyRegularCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendBlitFillThenInOrderPatchCmdsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0);

    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    uint32_t one = 1u;
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);
    CmdListMemoryCopyParams copyParams = {};

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint8_t), 4096u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    context->freeMem(dstBuffer);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenPatchingCommandsAfterCallingMemoryCopyRegionThenCommandsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);
    void *srcBuffer = reinterpret_cast<void *>(0x1234);
    void *dstBuffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    CmdListMemoryCopyParams copyParams = {};

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendMemoryCopyRegion(dstBuffer, &dr, width, 0,
                                        srcBuffer, &sr, width, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyCopyBlt, commandList->inOrderPatchCmds[0].patchCmdType);

    commandList->enablePatching(0);
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    auto &inOrderPatchCmd = commandList->inOrderPatchCmds[0];
    EXPECT_NE(nullptr, inOrderPatchCmd.cmd1);
    EXPECT_EQ(nullptr, inOrderPatchCmd.cmd2);
    XY_COPY_BLT copyBlt = *reinterpret_cast<XY_COPY_BLT *>(inOrderPatchCmd.cmd1);
    inOrderPatchCmd.patch(3);
    XY_COPY_BLT *modifiedBlt = reinterpret_cast<XY_COPY_BLT *>(inOrderPatchCmd.cmd1);
    EXPECT_EQ(memcmp(modifiedBlt, &copyBlt, sizeof(XY_COPY_BLT)), 0);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenPatchingCommandsAfterCallingMemoryCopyThenCommandsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);
    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyBlockCopyBlt, commandList->inOrderPatchCmds[0].patchCmdType);

    commandList->enablePatching(0);
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_BLOCK_COPY_BLT = typename GfxFamily::XY_BLOCK_COPY_BLT;
    auto &inOrderPatchCmd = commandList->inOrderPatchCmds[0];
    EXPECT_NE(nullptr, inOrderPatchCmd.cmd1);
    EXPECT_EQ(nullptr, inOrderPatchCmd.cmd2);
    XY_BLOCK_COPY_BLT copyBlt = *reinterpret_cast<XY_BLOCK_COPY_BLT *>(inOrderPatchCmd.cmd1);
    inOrderPatchCmd.patch(3);
    XY_BLOCK_COPY_BLT *modifiedBlt = reinterpret_cast<XY_BLOCK_COPY_BLT *>(inOrderPatchCmd.cmd1);
    EXPECT_EQ(memcmp(modifiedBlt, &copyBlt, sizeof(XY_BLOCK_COPY_BLT)), 0);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenPatchingCommandsAfterCallingMemoryFillWithTwoBytesPatternThenCommandsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);

    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    uint32_t one = 1u;
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);
    CmdListMemoryCopyParams copyParams = {};

    commandList->maxFillPaternSizeForCopyEngine = 4;

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint16_t), 4096u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyColorBlt, commandList->inOrderPatchCmds[0].patchCmdType);

    commandList->enablePatching(0);
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    auto &inOrderPatchCmd = commandList->inOrderPatchCmds[0];
    EXPECT_NE(nullptr, inOrderPatchCmd.cmd1);
    EXPECT_EQ(nullptr, inOrderPatchCmd.cmd2);
    XY_COLOR_BLT copyBlt = *reinterpret_cast<XY_COLOR_BLT *>(inOrderPatchCmd.cmd1);
    inOrderPatchCmd.patch(3);
    XY_COLOR_BLT *modifiedBlt = reinterpret_cast<XY_COLOR_BLT *>(inOrderPatchCmd.cmd1);
    EXPECT_EQ(memcmp(modifiedBlt, &copyBlt, sizeof(XY_COLOR_BLT)), 0);

    context->freeMem(dstBuffer);
}

HWTEST2_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenPatchingCommandsAfterCallingMemoryFillWithSingleBytePatternThenCommandsRemainsTheSame, IsAtLeastXeHpcCore) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);

    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    uint32_t one = 1u;
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);
    CmdListMemoryCopyParams copyParams = {};

    commandList->maxFillPaternSizeForCopyEngine = 4;

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint8_t), 4096u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::memSet, commandList->inOrderPatchCmds[0].patchCmdType);

    commandList->enablePatching(0);
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using MEM_SET = typename GfxFamily::MEM_SET;
    auto &inOrderPatchCmd = commandList->inOrderPatchCmds[0];
    EXPECT_NE(nullptr, inOrderPatchCmd.cmd1);
    EXPECT_EQ(nullptr, inOrderPatchCmd.cmd2);
    MEM_SET copyBlt = *reinterpret_cast<MEM_SET *>(inOrderPatchCmd.cmd1);
    inOrderPatchCmd.patch(3);
    MEM_SET *modifiedBlt = reinterpret_cast<MEM_SET *>(inOrderPatchCmd.cmd1);
    EXPECT_EQ(memcmp(modifiedBlt, &copyBlt, sizeof(MEM_SET)), 0);

    context->freeMem(dstBuffer);
}

struct AggregatedBcsSplitTests : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.SplitBcsAggregatedEventsMode.set(1);
        debugManager.flags.SplitBcsCopy.set(1);
        debugManager.flags.SplitBcsRequiredTileCount.set(expectedTileCount);
        debugManager.flags.SplitBcsRequiredEnginesCount.set(expectedEnginesCount);
        debugManager.flags.SplitBcsMask.set(0b11110);

        device = createDevice();
        context = Context::fromHandle(driverHandle->getDefaultContext());
        cmdList = createCmdList();
    }

    std::unique_ptr<L0::Device> createDevice() {
        ze_result_t returnValue;
        auto hwInfo = *NEO::defaultHwInfo;
        hwInfo.featureTable.ftrBcsInfo = 0b111111111;
        hwInfo.capabilityTable.blitterOperationsSupported = true;
        auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);

        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        auto device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), neoDevice, false, &returnValue));

        bcsSplit = &static_cast<DeviceImp *>(device.get())->bcsSplit;

        return device;
    }

    uint32_t queryCopyOrdinal() {
        uint32_t count = 0;
        device->getCommandQueueGroupProperties(&count, nullptr);

        std::vector<ze_command_queue_group_properties_t> groups;
        groups.resize(count);

        device->getCommandQueueGroupProperties(&count, groups.data());

        for (uint32_t i = 0; i < count; i++) {
            if (groups[i].flags == ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) {
                return i;
            }
        }

        EXPECT_TRUE(false);
        return 0;
    }

    std::unique_ptr<L0::CommandList> createCmdList() {
        ze_result_t returnValue;

        ze_command_queue_desc_t desc = {};
        desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
        desc.ordinal = queryCopyOrdinal();

        std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily,
                                                                                  device.get(),
                                                                                  &desc,
                                                                                  false,
                                                                                  NEO::EngineGroupType::copy,
                                                                                  returnValue));

        return commandList;
    }

    void *allocHostMem() {
        void *alloc = nullptr;
        ze_host_mem_alloc_desc_t deviceDesc = {};
        context->allocHostMem(&deviceDesc, copySize, 4096, &alloc);

        return alloc;
    }

    DebugManagerStateRestore restore;
    CmdListMemoryCopyParams copyParams = {};
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    std::unique_ptr<L0::Device> device;
    std::unique_ptr<L0::CommandList> cmdList;
    BcsSplit *bcsSplit = nullptr;
    Context *context = nullptr;
    const size_t copySize = 4 * MemoryConstants::megaByte;
    uint32_t expectedTileCount = 1;
    uint32_t expectedEnginesCount = 4;
};

HWTEST2_F(AggregatedBcsSplitTests, whenObtainCalledThenAggregatedEventsCreated, IsAtLeastXeHpcCore) {
    EXPECT_EQ(0u, bcsSplit->events.subcopy.size());
    EXPECT_TRUE(bcsSplit->events.aggregatedEventsMode);

    for (size_t i = 0; i < 8; i++) {
        auto index = bcsSplit->events.obtainForSplit(context, 123);
        ASSERT_TRUE(index.has_value());
        EXPECT_EQ(i, *index);

        EXPECT_EQ(0u, *bcsSplit->events.subcopy[i]->getInOrderExecInfo()->getBaseHostAddress());
        EXPECT_FALSE(bcsSplit->events.subcopy[i]->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST));
        EXPECT_TRUE(bcsSplit->events.subcopy[i]->isSignalScope(ZE_EVENT_SCOPE_FLAG_DEVICE));
        EXPECT_EQ(1u, bcsSplit->events.subcopy[i]->getInOrderIncrementValue());
        EXPECT_EQ(static_cast<uint64_t>(bcsSplit->cmdQs.size()), bcsSplit->events.subcopy[i]->getInOrderExecBaseSignalValue());

        EXPECT_EQ(nullptr, bcsSplit->events.marker[i]->getInOrderExecInfo());
        EXPECT_TRUE(bcsSplit->events.marker[i]->isCounterBased());
        EXPECT_TRUE(bcsSplit->events.marker[i]->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST));
        EXPECT_FALSE(bcsSplit->events.marker[i]->isSignalScope(ZE_EVENT_SCOPE_FLAG_DEVICE));

        // already reserved for this obtainForSplit() call
        EXPECT_EQ(ZE_RESULT_NOT_READY, bcsSplit->events.marker[i]->queryStatus());

        EXPECT_EQ(8u, bcsSplit->events.subcopy.size());
        EXPECT_EQ(1u, bcsSplit->events.allocsForAggregatedEvents.size());
        EXPECT_EQ(8u, bcsSplit->events.marker.size());
        EXPECT_EQ(0u, bcsSplit->events.barrier.size());
    }

    auto index = bcsSplit->events.obtainForSplit(context, 123);
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(8u, *index);
    EXPECT_EQ(16u, bcsSplit->events.subcopy.size());
    EXPECT_EQ(16u, bcsSplit->events.marker.size());
    EXPECT_EQ(1u, bcsSplit->events.allocsForAggregatedEvents.size());

    for (size_t i = 0; i < 16; i++) {
        EXPECT_EQ(0u, *bcsSplit->events.subcopy[i]->getInOrderExecInfo()->getBaseHostAddress());

        if (i <= 8) {
            EXPECT_EQ(ZE_RESULT_NOT_READY, bcsSplit->events.marker[i]->queryStatus());
        } else {
            EXPECT_EQ(ZE_RESULT_SUCCESS, bcsSplit->events.marker[i]->queryStatus());
        }
    }

    bcsSplit->events.resetAggregatedEventState(1, true);

    index = bcsSplit->events.obtainForSplit(context, 123);
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(1u, *index);
    EXPECT_EQ(16u, bcsSplit->events.subcopy.size());
    EXPECT_EQ(16u, bcsSplit->events.marker.size());
    EXPECT_EQ(1u, bcsSplit->events.allocsForAggregatedEvents.size());

    for (auto &event : bcsSplit->events.subcopy) {
        EXPECT_TRUE(event->isCounterBased());
        EXPECT_EQ(1u, event->getInOrderIncrementValue());
        EXPECT_EQ(static_cast<uint64_t>(bcsSplit->cmdQs.size()), event->getInOrderExecSignalValueWithSubmissionCounter());
    }
}

HWTEST2_F(AggregatedBcsSplitTests, givenMultipleEventsWhenObtainIsCalledTheAssignNewDeviceAlloc, IsAtLeastXeHpcCore) {
    auto index = bcsSplit->events.obtainForSplit(context, 123);
    EXPECT_EQ(8u, bcsSplit->events.subcopy.size());
    ASSERT_EQ(1u, bcsSplit->events.allocsForAggregatedEvents.size());
    auto alloc = bcsSplit->events.allocsForAggregatedEvents[0];

    for (size_t i = 0; i < bcsSplit->events.subcopy.size(); i++) {
        EXPECT_EQ(castToUint64(ptrOffset(alloc, (MemoryConstants::cacheLineSize * i))), bcsSplit->events.subcopy[i]->getInOrderExecInfo()->getBaseDeviceAddress());
        EXPECT_EQ(ptrOffset(alloc, (MemoryConstants::cacheLineSize * i)), bcsSplit->events.subcopy[i]->getInOrderExecInfo()->getBaseHostAddress());
    }

    bcsSplit->events.currentAggregatedAllocOffset = MemoryConstants::pageSize64k - (MemoryConstants::cacheLineSize - 1);

    while (bcsSplit->events.subcopy.size() == 8) {
        index = bcsSplit->events.obtainForSplit(context, 123);
    }

    EXPECT_EQ(16u, bcsSplit->events.subcopy.size());
    EXPECT_EQ(8u, *index);

    ASSERT_EQ(2u, bcsSplit->events.allocsForAggregatedEvents.size());
    auto alloc2 = bcsSplit->events.allocsForAggregatedEvents[1];

    for (size_t i = 0; i < 8; i++) {
        EXPECT_EQ(castToUint64(ptrOffset(alloc, (MemoryConstants::cacheLineSize * i))), bcsSplit->events.subcopy[i]->getInOrderExecInfo()->getBaseDeviceAddress());
        EXPECT_EQ(ptrOffset(alloc, (MemoryConstants::cacheLineSize * i)), bcsSplit->events.subcopy[i]->getInOrderExecInfo()->getBaseHostAddress());
    }

    for (size_t i = 8; i < 16; i++) {
        auto offset = MemoryConstants::cacheLineSize * (i - 8);

        EXPECT_EQ(castToUint64(ptrOffset(alloc2, offset)), bcsSplit->events.subcopy[i]->getInOrderExecInfo()->getBaseDeviceAddress());
        EXPECT_EQ(ptrOffset(alloc2, offset), bcsSplit->events.subcopy[i]->getInOrderExecInfo()->getBaseHostAddress());
    }
}

HWTEST2_F(AggregatedBcsSplitTests, givenMarkerEventWhenCheckingCompletionThenResetItsState, IsAtLeastXeHpcCore) {
    auto ptr = allocHostMem();

    auto cmdListHw = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(cmdList.get());

    cmdListHw->appendMemoryCopy(ptr, ptr, copySize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdListHw->inOrderExecInfo.get(), bcsSplit->events.marker[0]->getInOrderExecInfo().get());
    EXPECT_EQ(cmdListHw->inOrderExecInfo->getCounterValue(), bcsSplit->events.marker[0]->getInOrderExecBaseSignalValue());

    cmdListHw->appendMemoryCopy(ptr, ptr, copySize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdListHw->inOrderExecInfo.get(), bcsSplit->events.marker[1]->getInOrderExecInfo().get());
    EXPECT_EQ(cmdListHw->inOrderExecInfo->getCounterValue(), bcsSplit->events.marker[1]->getInOrderExecBaseSignalValue());

    *cmdListHw->inOrderExecInfo->getBaseHostAddress() = 2;

    cmdListHw->appendMemoryCopy(ptr, ptr, copySize, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(nullptr, bcsSplit->events.marker[2]->getInOrderExecInfo().get());
    EXPECT_EQ(cmdListHw->inOrderExecInfo.get(), bcsSplit->events.marker[0]->getInOrderExecInfo().get());
    EXPECT_EQ(cmdListHw->inOrderExecInfo->getCounterValue(), bcsSplit->events.marker[0]->getInOrderExecBaseSignalValue());

    context->freeMem(ptr);
}

struct MultiTileAggregatedBcsSplitTests : public AggregatedBcsSplitTests {
    void SetUp() override {
        expectedTileCount = 2;
        expectedEnginesCount *= expectedTileCount;
        debugManager.flags.CreateMultipleSubDevices.set(expectedTileCount);
        AggregatedBcsSplitTests::SetUp();
        EXPECT_EQ(expectedTileCount, device->getNEODevice()->getNumSubDevices());
    }
};

HWTEST2_F(MultiTileAggregatedBcsSplitTests, givenMuliTileBcsSplitWhenSetupingThenCreateCorrectQueues, IsAtLeastXeHpcCore) {
    ASSERT_EQ(expectedEnginesCount, bcsSplit->cmdQs.size());

    auto perTileEngineCount = expectedEnginesCount / expectedTileCount;

    for (uint32_t tileId = 0; tileId < expectedTileCount; tileId++) {
        for (uint32_t baseEngineId = 0; baseEngineId < perTileEngineCount; baseEngineId++) {
            auto engineId = (tileId * perTileEngineCount) + baseEngineId;

            auto expectedEngineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_BCS1 + baseEngineId);

            auto &osContext = static_cast<CommandQueueImp *>(bcsSplit->cmdQs[engineId])->getCsr()->getOsContext();
            EXPECT_EQ(expectedEngineType, osContext.getEngineType());
            EXPECT_EQ(1u << tileId, osContext.getDeviceBitfield().to_ulong());
        }
    }
}

HWTEST2_F(MultiTileAggregatedBcsSplitTests, givenIncorrectNumberOfTilesWhenCreatingBcsSplitThenDontCreate, IsAtLeastXeHpcCore) {
    debugManager.flags.SplitBcsRequiredTileCount.set(expectedTileCount + 1);
    cmdList.reset();
    bcsSplit->releaseResources();
    EXPECT_EQ(0u, bcsSplit->cmdQs.size());

    cmdList = createCmdList();
    EXPECT_EQ(0u, bcsSplit->cmdQs.size());
}

} // namespace ult
} // namespace L0
