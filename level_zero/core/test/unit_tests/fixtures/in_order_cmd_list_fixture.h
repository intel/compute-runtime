/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/event/event_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

namespace L0 {
namespace ult {

struct InOrderCmdListFixture : public ::Test<ModuleFixture> {
    struct FixtureMockEvent : public EventImp<uint32_t> {
        using EventImp<uint32_t>::Event::counterBasedMode;
        using EventImp<uint32_t>::maxPacketCount;
        using EventImp<uint32_t>::inOrderExecInfo;
        using EventImp<uint32_t>::inOrderExecSignalValue;
        using EventImp<uint32_t>::inOrderAllocationOffset;
        using EventImp<uint32_t>::csrs;
        using EventImp<uint32_t>::signalScope;

        void makeCounterBasedInitiallyDisabled() {
            counterBasedMode = CounterBasedMode::InitiallyDisabled;
        }

        void makeCounterBasedImplicitlyDisabled() {
            counterBasedMode = CounterBasedMode::ImplicitlyDisabled;
        }
    };

    void SetUp() override {
        NEO::debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(NEO::PreemptionMode::Disabled));

        ::Test<ModuleFixture>::SetUp();
        createKernel();

        const_cast<KernelDescriptor &>(kernel->getKernelDescriptor()).kernelAttributes.flags.usesPrintf = false;
    }

    void TearDown() override {
        events.clear();

        ::Test<ModuleFixture>::TearDown();
    }

    template <typename GfxFamily>
    std::unique_ptr<L0::EventPool> createEvents(uint32_t numEvents, bool timestampEvent) {
        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        eventPoolDesc.count = numEvents;

        ze_event_pool_counter_based_exp_desc_t counterBasedExtension = {ZE_STRUCTURE_TYPE_COUNTER_BASED_EVENT_POOL_EXP_DESC};
        eventPoolDesc.pNext = &counterBasedExtension;

        if (timestampEvent) {
            eventPoolDesc.flags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
        }

        ze_event_desc_t eventDesc = {};
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

        auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

        for (uint32_t i = 0; i < numEvents; i++) {
            eventDesc.index = i;
            events.emplace_back(DestroyableZeUniquePtr<FixtureMockEvent>(static_cast<FixtureMockEvent *>(Event::create<typename GfxFamily::TimestampPacketType>(eventPool.get(), &eventDesc, device))));
            EXPECT_EQ(Event::CounterBasedMode::ExplicitlyEnabled, events.back()->counterBasedMode);
            EXPECT_TRUE(events.back()->isCounterBased());
        }

        return eventPool;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>> createImmCmdList() {
        return createImmCmdListImpl<gfxCoreFamily, WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    }

    template <GFXCORE_FAMILY gfxCoreFamily, typename CmdListT>
    DestroyableZeUniquePtr<CmdListT> createImmCmdListImpl() {
        auto cmdList = makeZeUniquePtr<CmdListT>();

        auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

        ze_command_queue_desc_t desc = {};
        desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;

        mockCmdQs.emplace_back(std::make_unique<Mock<CommandQueue>>(device, csr, &desc));

        cmdList->cmdQImmediate = mockCmdQs[createdCmdLists].get();
        cmdList->isFlushTaskSubmissionEnabled = true;
        cmdList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;
        cmdList->csr = csr;
        cmdList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
        cmdList->commandContainer.setImmediateCmdListCsr(csr);
        cmdList->enableInOrderExecution();

        createdCmdLists++;

        return cmdList;
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

        mockCopyOsContext = std::make_unique<NEO::MockOsContext>(0, NEO::EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, DeviceBitfield(1)));
        cmdList->csr->setupContext(*mockCopyOsContext);
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
    bool verifyInOrderDependency(GenCmdList::iterator &cmd, uint64_t counter, uint64_t syncVa, bool qwordCounter);

    DebugManagerStateRestore restorer;
    std::unique_ptr<NEO::MockOsContext> mockCopyOsContext;

    uint32_t createdCmdLists = 0;
    std::vector<DestroyableZeUniquePtr<FixtureMockEvent>> events;
    std::vector<std::unique_ptr<Mock<CommandQueue>>> mockCmdQs;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount = {3, 2, 1};
    CmdListKernelLaunchParams launchParams = {};
};

template <typename GfxFamily>
bool InOrderCmdListFixture::verifyInOrderDependency(GenCmdList::iterator &cmd, uint64_t counter, uint64_t syncVa, bool qwordCounter) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    if (qwordCounter) {
        auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(*cmd);
        if (!lri) {
            return false;
        }
        EXPECT_EQ(getLowPart(counter), lri->getDataDword());
        EXPECT_EQ(RegisterOffsets::csGprR0, lri->getRegisterOffset());

        lri++;

        EXPECT_EQ(getHighPart(counter), lri->getDataDword());
        EXPECT_EQ(RegisterOffsets::csGprR0 + 4, lri->getRegisterOffset());

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
} // namespace ult
} // namespace L0