/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/event/event_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"
#include "level_zero/driver_experimental/zex_api.h"

namespace L0 {
namespace ult {

struct InOrderFixtureMockEvent : public EventImp<uint32_t> {
    using EventImp<uint32_t>::Event::counterBasedMode;
    using EventImp<uint32_t>::Event::isFromIpcPool;
    using EventImp<uint32_t>::Event::counterBasedFlags;
    using EventImp<uint32_t>::Event::isSharableCounterBased;
    using EventImp<uint32_t>::Event::isTimestampEvent;
    using EventImp<uint32_t>::eventPoolAllocation;
    using EventImp<uint32_t>::maxPacketCount;
    using EventImp<uint32_t>::inOrderExecInfo;
    using EventImp<uint32_t>::inOrderExecSignalValue;
    using EventImp<uint32_t>::inOrderAllocationOffset;
    using EventImp<uint32_t>::csrs;
    using EventImp<uint32_t>::signalScope;
    using EventImp<uint32_t>::waitScope;
    using EventImp<uint32_t>::unsetCmdQueue;
    using EventImp<uint32_t>::externalInterruptId;
    using EventImp<uint32_t>::latestUsedCmdQueue;
    using EventImp<uint32_t>::inOrderTimestampNode;
    using EventImp<uint32_t>::additionalTimestampNode;
    using EventImp<uint32_t>::isCompleted;

    void makeCounterBasedInitiallyDisabled(MultiGraphicsAllocation &poolAllocation) {
        resetInOrderTimestampNode(nullptr, 0);
        counterBasedMode = CounterBasedMode::initiallyDisabled;
        resetCompletionStatus();
        counterBasedFlags = 0;
        this->eventPoolAllocation = &poolAllocation;
        this->hostAddressFromPool = ptrOffset(eventPoolAllocation->getGraphicsAllocation(0)->getUnderlyingBuffer(), eventPoolOffset);
        reset();
    }

    void makeCounterBasedImplicitlyDisabled(MultiGraphicsAllocation &poolAllocation) {
        resetInOrderTimestampNode(nullptr, 0);
        counterBasedMode = CounterBasedMode::implicitlyDisabled;
        resetCompletionStatus();
        counterBasedFlags = 0;
        this->eventPoolAllocation = &poolAllocation;
        this->hostAddressFromPool = ptrOffset(eventPoolAllocation->getGraphicsAllocation(0)->getUnderlyingBuffer(), eventPoolOffset);
        reset();
    }
};

class WhiteboxInOrderExecInfo : public InOrderExecInfo {
  public:
    using InOrderExecInfo::numDevicePartitionsToWait;
    using InOrderExecInfo::numHostPartitionsToWait;
    using InOrderExecInfo::tempTimestampNodes;
};

struct InOrderCmdListFixture : public ::Test<ModuleFixture> {
    void SetUp() override {
        NEO::debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(NEO::PreemptionMode::Disabled));
        NEO::debugManager.flags.ResolveDependenciesViaPipeControls.set(0u);

        ::Test<ModuleFixture>::SetUp();
        createKernel();

        const_cast<KernelDescriptor &>(kernel->getKernelDescriptor()).kernelAttributes.flags.usesPrintf = false;
    }

    void TearDown() override {
        events.clear();

        ::Test<ModuleFixture>::TearDown();
    }

    DestroyableZeUniquePtr<InOrderFixtureMockEvent> createExternalSyncStorageEvent(uint64_t counterValue, uint64_t incrementValue, uint64_t *deviceAddress) {
        ze_event_handle_t outEvent = nullptr;
        zex_counter_based_event_external_storage_properties_t externalStorageAllocProperties = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_STORAGE_ALLOC_PROPERTIES};
        externalStorageAllocProperties.completionValue = counterValue;
        externalStorageAllocProperties.deviceAddress = deviceAddress;
        externalStorageAllocProperties.incrementValue = incrementValue;

        zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
        counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE;
        counterBasedDesc.pNext = &externalStorageAllocProperties;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &outEvent));

        auto eventObj = static_cast<InOrderFixtureMockEvent *>(Event::fromHandle(outEvent));

        return DestroyableZeUniquePtr<InOrderFixtureMockEvent>(eventObj);
    }

    DestroyableZeUniquePtr<InOrderFixtureMockEvent> createStandaloneCbEvent(const ze_base_desc_t *pNext) {
        constexpr uint32_t counterBasedFlags = (ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE | ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE);

        const EventDescriptor eventDescriptor = {
            .eventPoolAllocation = nullptr,
            .extensions = pNext,
            .totalEventSize = 0,
            .maxKernelCount = EventPacketsCount::maxKernelSplit,
            .maxPacketsCount = 0,
            .counterBasedFlags = counterBasedFlags,
            .index = 0,
            .signalScope = 0,
            .waitScope = 0,
            .timestampPool = false,
            .kernelMappedTsPoolFlag = false,
            .importedIpcPool = false,
            .ipcPool = false,
        };

        standaloneCbEventStorage.push_back(1);

        uint64_t *hostAddress = &(standaloneCbEventStorage.data()[standaloneCbEventStorage.size() - 1]);
        uint64_t *deviceAddress = ptrOffset(hostAddress, 0x1000);

        auto inOrderExecInfo = NEO::InOrderExecInfo::createFromExternalAllocation(*device->getNEODevice(), nullptr, castToUint64(deviceAddress), nullptr, hostAddress, 1, 1, 1);

        ze_result_t result = ZE_RESULT_SUCCESS;
        auto event = static_cast<InOrderFixtureMockEvent *>(Event::create<uint64_t>(eventDescriptor, device, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        event->updateInOrderExecState(inOrderExecInfo, 1, 0);

        return DestroyableZeUniquePtr<InOrderFixtureMockEvent>(event);
    }

    template <typename GfxFamily>
    std::unique_ptr<L0::EventPool> createEvents(uint32_t numEvents, bool timestampEvent) {
        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        eventPoolDesc.count = numEvents;

        ze_event_pool_counter_based_exp_desc_t counterBasedExtension = {ZE_STRUCTURE_TYPE_COUNTER_BASED_EVENT_POOL_EXP_DESC};
        counterBasedExtension.flags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE | ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE;
        eventPoolDesc.pNext = &counterBasedExtension;

        if (timestampEvent) {
            eventPoolDesc.flags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
        }

        ze_event_desc_t eventDesc = {};
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

        auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

        for (uint32_t i = 0; i < numEvents; i++) {
            eventDesc.index = i;
            events.emplace_back(DestroyableZeUniquePtr<InOrderFixtureMockEvent>(static_cast<InOrderFixtureMockEvent *>(Event::create<typename GfxFamily::TimestampPacketType>(eventPool.get(), &eventDesc, device))));
            EXPECT_EQ(Event::CounterBasedMode::explicitlyEnabled, events.back()->counterBasedMode);
            EXPECT_TRUE(events.back()->isCounterBased());
            EXPECT_EQ(counterBasedExtension.flags, events.back()->counterBasedFlags);
        }

        return eventPool;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createImmCmdList() {
        return createImmCmdListImpl<gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>(false);
    }

    template <GFXCORE_FAMILY gfxCoreFamily, typename CmdListT>
    DestroyableZeUniquePtr<CmdListT> createImmCmdListImpl(bool copyOffloadEnabled) {
        auto cmdList = makeZeUniquePtr<CmdListT>();

        auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

        ze_command_queue_desc_t desc = {};
        desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;

        mockCmdQs.emplace_back(std::make_unique<Mock<CommandQueue>>(device, csr, &desc));

        cmdList->cmdQImmediate = mockCmdQs[createdCmdLists].get();
        cmdList->cmdListType = CommandList::CommandListType::typeImmediate;
        cmdList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
        cmdList->commandContainer.setImmediateCmdListCsr(csr);
        cmdList->enableInOrderExecution();

        if (copyOffloadEnabled) {
            cmdList->enableCopyOperationOffload();
            cmdList->copyOperationFenceSupported = device->getProductHelper().isDeviceToHostCopySignalingFenceRequired();
        }

        completeHostAddress<gfxCoreFamily>(cmdList.get());

        createdCmdLists++;

        return cmdList;
    }

    template <GFXCORE_FAMILY gfxCoreFamily, typename CmdListT>
    void completeHostAddress(CmdListT *cmdList) {
        uint64_t maxValue = std::numeric_limits<uint64_t>::max();
        void *hostAddress = ptrOffset(cmdList->inOrderExecInfo->getBaseHostAddress(), cmdList->inOrderExecInfo->getAllocationOffset());
        for (uint32_t i = 0; i < cmdList->inOrderExecInfo->getNumHostPartitionsToWait(); i++) {
            memcpy(hostAddress, &maxValue, sizeof(maxValue));
            hostAddress = ptrOffset(hostAddress, device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());
        }
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamily<gfxCoreFamily>>> createRegularCmdList(bool copyOnly) {
        auto cmdList = makeZeUniquePtr<WhiteBox<L0::CommandListCoreFamily<gfxCoreFamily>>>();

        auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

        ze_command_queue_desc_t desc = {};

        mockCmdQs.emplace_back(std::make_unique<Mock<CommandQueue>>(device, csr, &desc));

        auto engineType = copyOnly ? EngineGroupType::copy : EngineGroupType::renderCompute;

        cmdList->initialize(device, engineType, ZE_COMMAND_LIST_FLAG_IN_ORDER);

        createdCmdLists++;

        return cmdList;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createCopyOnlyImmCmdList() {
        auto cmdList = createImmCmdList<gfxCoreFamily>();

        cmdList->engineGroupType = EngineGroupType::copy;

        mockCopyOsContext = std::make_unique<NEO::MockOsContext>(0, NEO::EngineDescriptorHelper::getDefaultDescriptor({device->getProductHelper().getDefaultCopyEngine(), EngineUsage::regular}, DeviceBitfield(1)));
        cmdList->getCsr(false)->setupContext(*mockCopyOsContext);
        return cmdList;
    }

    template <typename FamilyType>
    GenCmdList::iterator findBltFillCmd(GenCmdList::iterator begin, GenCmdList::iterator end) {
        using XY_COPY_BLT = typename std::remove_const<decltype(FamilyType::cmdInitXyCopyBlt)>::type;

        if constexpr (!std::is_same<XY_COPY_BLT, typename FamilyType::XY_BLOCK_COPY_BLT>::value) {
            auto fillItor = find<typename FamilyType::MEM_SET *>(begin, end);
            if (fillItor != end) {
                return fillItor;
            }
        }

        return find<typename FamilyType::XY_COLOR_BLT *>(begin, end);
    }

    void *allocHostMem(size_t size) {
        ze_host_mem_alloc_desc_t desc = {};
        void *ptr = nullptr;
        context->allocHostMem(&desc, size, 1, &ptr);

        return ptr;
    }

    void *allocDeviceMem(size_t size) {
        void *alloc = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        context->allocDeviceMem(device->toHandle(), &deviceDesc, size, 4096u, &alloc);

        return alloc;
    }

    template <typename GfxFamily>
    bool verifyInOrderDependency(GenCmdList::iterator &cmd, uint64_t counter, uint64_t syncVa, bool qwordCounter, bool isBcs);

    DebugManagerStateRestore restorer;
    std::unique_ptr<NEO::MockOsContext> mockCopyOsContext;

    uint32_t createdCmdLists = 0;
    std::vector<DestroyableZeUniquePtr<InOrderFixtureMockEvent>> events;
    std::vector<std::unique_ptr<Mock<CommandQueue>>> mockCmdQs;
    std::vector<uint64_t> standaloneCbEventStorage;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount = {3, 2, 1};
    CmdListKernelLaunchParams launchParams = {};
    CmdListMemoryCopyParams copyParams = {};
};

template <typename GfxFamily>
bool InOrderCmdListFixture::verifyInOrderDependency(GenCmdList::iterator &cmd, uint64_t counter, uint64_t syncVa, bool qwordCounter, bool isBcs) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    if (qwordCounter) {
        auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(*cmd);
        if (!lri) {
            return false;
        }

        uint32_t base = (isBcs) ? RegisterOffsets::bcs0Base : 0x0;
        EXPECT_EQ(getLowPart(counter), lri->getDataDword());
        EXPECT_EQ(RegisterOffsets::csGprR0 + base, lri->getRegisterOffset());

        lri++;

        EXPECT_EQ(getHighPart(counter), lri->getDataDword());
        EXPECT_EQ(RegisterOffsets::csGprR0 + 4 + base, lri->getRegisterOffset());

        std::advance(cmd, 2);
    }

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*cmd);
    if (!semaphoreCmd) {
        return false;
    }

    EXPECT_EQ(syncVa, semaphoreCmd->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, semaphoreCmd->getCompareOperation());

    if (qwordCounter) {
        EXPECT_EQ(0u, semaphoreCmd->getSemaphoreDataDword());
    } else {
        EXPECT_EQ(0u, getHighPart(counter));
        EXPECT_EQ(getLowPart(counter), semaphoreCmd->getSemaphoreDataDword());
    }

    cmd++;
    return true;
}

struct MultiTileInOrderCmdListFixture : public InOrderCmdListFixture {
    void SetUp() override {
        NEO::debugManager.flags.CreateMultipleSubDevices.set(partitionCount);
        NEO::debugManager.flags.EnableImplicitScaling.set(4);

        InOrderCmdListFixture::SetUp();
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createMultiTileImmCmdList() {
        auto cmdList = createImmCmdList<gfxCoreFamily>();

        cmdList->partitionCount = partitionCount;

        return cmdList;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamily<gfxCoreFamily>>> createMultiTileRegularCmdList(bool copyOnly) {
        auto cmdList = createRegularCmdList<gfxCoreFamily>(copyOnly);

        cmdList->partitionCount = partitionCount;

        return cmdList;
    }

    const uint32_t partitionCount = 2;
};

struct MultiTileSynchronizedDispatchFixture : public MultiTileInOrderCmdListFixture {
    void SetUp() override {
        NEO::debugManager.flags.ForceSynchronizedDispatchMode.set(1);
        MultiTileInOrderCmdListFixture::SetUp();
    }
};

} // namespace ult
} // namespace L0
