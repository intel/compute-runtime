/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_helper.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/core/ze_mutable_cmdlist_api_entrypoints.h"
#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_internal_options.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mcl_encoder.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_load_register_imm_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_pipe_control_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_semaphore_wait_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_store_data_imm_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_store_register_mem_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_variable.h"
#include "level_zero/experimental/source/mutable_cmdlist/program/mcl_decoder.h"

#include "copy_offload_mode.h"

#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace L0 {
namespace ult {

using MutableCommandListTest = Test<MutableCommandListFixture<false, -1>>;

using MutableCommandListInOrderTest = Test<MutableCommandListFixture<true, -1>>;
using MutableCommandListInOrderSem64Test = Test<MutableCommandListFixture<true, 1>>;
using MutableCommandListInOrderNoSem64Test = Test<MutableCommandListFixture<true, 0>>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListWhenAppendingKernelWithSignalEventRegularTimestampAndMutateItThenExpectPatchNewEventAddress) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerType = typename FamilyType::PorWalkerType;

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(false, false, true, false, false);
    auto newEvent = createTestEvent(false, false, true, false, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    auto baseGpuVa = event->getGpuAddress(this->device);
    auto completionGpuVa = event->getCompletionFieldGpuAddress(this->device);

    auto walkerCmd = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    auto walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);

    MI_STORE_DATA_IMM *sdiCmd = nullptr;
    MI_SEMAPHORE_WAIT *semCmd = nullptr;
    if (mockBaseCmdListHw->duplicatedInOrderCounterStorageEnabled == false) {
        ASSERT_EQ(1u, signalEvents[0]->getStoreDataImmList().size());
        auto mutableSdi = signalEvents[0]->getStoreDataImmList()[0];
        auto mockMutableSdi = static_cast<MockMutableStoreDataImmHw<FamilyType> *>(mutableSdi);
        sdiCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(mockMutableSdi->storeDataImm);
        EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());

        ASSERT_EQ(1u, signalEvents[0]->getSemWaitList().size());
        auto mutableSem = signalEvents[0]->getSemWaitList()[0];
        auto mockMutableSem = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSem);
        semCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSem->semWait);
        EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));
    }

    result = mutableCommandList->updateMutableCommandSignalEvent(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    baseGpuVa = newEvent->getGpuAddress(this->device);
    completionGpuVa = newEvent->getCompletionFieldGpuAddress(this->device);

    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);

    if (mockBaseCmdListHw->duplicatedInOrderCounterStorageEnabled == false) {
        EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());
        EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListOnDcFlushPlatformWhenAppendingKernelWithSignalEventRegularSignalScopeAndMutateItThenExpectPatchNewEventAddress) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto &productHelper = this->neoDevice->getProductHelper();
    if (productHelper.isL3FlushAfterPostSyncSupported()) {
        GTEST_SKIP();
    }

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);
    mockBaseCmdListHw->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(false, true, false, false, false);
    auto newEvent = createTestEvent(false, true, false, false, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    auto baseGpuVa = event->getGpuAddress(this->device);
    auto completionGpuVa = event->getCompletionFieldGpuAddress(this->device);

    MI_STORE_DATA_IMM *sdiCmd = nullptr;
    MI_SEMAPHORE_WAIT *semCmd = nullptr;
    PIPE_CONTROL *pipeControlCmd = nullptr;

    ASSERT_EQ(1u, signalEvents[0]->getStoreDataImmList().size());
    auto mutableSdi = signalEvents[0]->getStoreDataImmList()[0];
    auto mockMutableSdi = static_cast<MockMutableStoreDataImmHw<FamilyType> *>(mutableSdi);
    sdiCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(mockMutableSdi->storeDataImm);
    EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());

    ASSERT_EQ(1u, signalEvents[0]->getSemWaitList().size());
    auto mutableSem = signalEvents[0]->getSemWaitList()[0];
    auto mockMutableSem = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSem);
    semCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSem->semWait);
    EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));

    ASSERT_EQ(1u, mutableCommandList->mutablePipeControlCmds.size());
    auto mockMutablePipeControl = static_cast<MockMutablePipeControlHw<FamilyType> *>(mutableCommandList->mutablePipeControlCmds[0].get());
    pipeControlCmd = reinterpret_cast<PIPE_CONTROL *>(mockMutablePipeControl->pipeControl);
    auto postSyncAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd);
    EXPECT_EQ(baseGpuVa, postSyncAddress);

    result = mutableCommandList->updateMutableCommandSignalEvent(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    baseGpuVa = newEvent->getGpuAddress(this->device);
    completionGpuVa = newEvent->getCompletionFieldGpuAddress(this->device);

    EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());
    EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));
    postSyncAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd);
    EXPECT_EQ(baseGpuVa, postSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListOnDcFlushPlatformWhenAppendingKernelWithSignalEventRegularSignalScopeTimestampAndMutateItThenExpectPatchNewEventAddress) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    auto &productHelper = this->neoDevice->getProductHelper();
    if (productHelper.isL3FlushAfterPostSyncSupported()) {
        GTEST_SKIP();
    }

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);
    mockBaseCmdListHw->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(false, true, true, false, false);
    auto newEvent = createTestEvent(false, true, true, false, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());
    auto baseGpuVa = event->getGpuAddress(this->device);
    auto completionGpuVa = event->getCompletionFieldGpuAddress(this->device);

    MI_STORE_DATA_IMM *sdiCmd = nullptr;
    MI_STORE_REGISTER_MEM *srmBaseCmd = nullptr;
    MI_STORE_REGISTER_MEM *srmComplCmd = nullptr;
    MI_SEMAPHORE_WAIT *semCmd = nullptr;

    ASSERT_EQ(1u, signalEvents[0]->getStoreDataImmList().size());
    auto mutableSdi = signalEvents[0]->getStoreDataImmList()[0];
    auto mockMutableSdi = static_cast<MockMutableStoreDataImmHw<FamilyType> *>(mutableSdi);
    sdiCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(mockMutableSdi->storeDataImm);
    EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());

    ASSERT_EQ(1u, signalEvents[0]->getSemWaitList().size());
    auto mutableSem = signalEvents[0]->getSemWaitList()[0];
    auto mockMutableSem = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSem);
    semCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSem->semWait);
    EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));

    for (auto &mutableStoreRegMem : signalEvents[0]->getStoreRegMemList()) {
        auto mockMutableStoreRegMem = static_cast<MockMutableStoreRegisterMemHw<FamilyType> *>(mutableStoreRegMem);
        auto storeRegMemCmd = reinterpret_cast<MI_STORE_REGISTER_MEM *>(mockMutableStoreRegMem->storeRegMem);
        if (storeRegMemCmd->getMemoryAddress() == baseGpuVa) {
            srmBaseCmd = storeRegMemCmd;
        }
        if (storeRegMemCmd->getMemoryAddress() == completionGpuVa) {
            srmComplCmd = storeRegMemCmd;
        }
    }
    ASSERT_NE(nullptr, srmBaseCmd);
    ASSERT_NE(nullptr, srmComplCmd);

    result = mutableCommandList->updateMutableCommandSignalEvent(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    baseGpuVa = newEvent->getGpuAddress(this->device);
    completionGpuVa = newEvent->getCompletionFieldGpuAddress(this->device);

    EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());
    EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));
    EXPECT_EQ(completionGpuVa, srmComplCmd->getMemoryAddress());
    EXPECT_EQ(baseGpuVa, srmBaseCmd->getMemoryAddress());
}

template <typename FamilyType>
void MutableCommandListFixtureInit::waitCbEventBelongToCurrentMutateToDifferent() {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t lriNoopSpace[sizeof(MI_LOAD_REGISTER_IMM)] = {0};
    alignas(uint32_t) uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto signalEvent = this->createTestEvent(false, false, false, false, false);
    auto signalEventHandle = signalEvent->toHandle();
    auto event = this->createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = this->createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto externalCmdList = this->createMutableCmdList();
    // attach event 2 to the external command list
    auto result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // attach event 1 to the command list
    result = this->mutableCommandList->appendLaunchKernel(this->kernel->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT | ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event and signal event 0
    result = this->mutableCommandList->appendLaunchKernel(this->kernel->toHandle(), this->testGroupCount, signalEventHandle, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = this->getVariableList(this->commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    const size_t expectedLriSize = this->lriRequired ? 2 : 0;

    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;
    if (expectedLriSize > 0) {
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));

        mutableLri = waitEventVar->getLoadRegImmList()[1];
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriUpperCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    result = this->mutableCommandList->updateMutableCommandWaitEvents(this->commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitAddress = newEvent->getInOrderExecEventHelper().getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    if (expectedLriSize > 0) {
        constexpr uint32_t firstRegister = 0x2600;
        constexpr uint32_t secondRegister = 0x2604;

        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderNoSem64Test,
            givenSemaphore64OffAndKernelWithWaitCbEventBelongingToCmdListWhenMutateIntoEventFromDifferentCmdListThenDataIsUpdatedAndCommandResotred) {
    waitCbEventBelongToCurrentMutateToDifferent<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderSem64Test,
            givenSemaphore64OnAndKernelWithWaitCbEventBelongingToCmdListWhenMutateIntoEventFromDifferentCmdListThenDataIsUpdatedAndCommandResotred) {
    waitCbEventBelongToCurrentMutateToDifferent<FamilyType>();
}

template <typename FamilyType>
void MutableCommandListFixtureInit::waitCbEventBelongToCurrentMutateToCurrent() {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t lriNoopSpace[sizeof(MI_LOAD_REGISTER_IMM)] = {0};
    alignas(uint32_t) uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = this->createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = this->createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    // attach events 1 & 2 to the command list
    auto result = this->mutableCommandList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = this->mutableCommandList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = this->mutableCommandList->appendLaunchKernel(this->kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = this->getVariableList(this->commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    const size_t expectedLriSize = this->lriRequired ? 2 : 0;
    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;
    if (expectedLriSize > 0) {
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));

        mutableLri = waitEventVar->getLoadRegImmList()[1];
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriUpperCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    result = this->mutableCommandList->updateMutableCommandWaitEvents(this->commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));
    if (expectedLriSize > 0) {
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderNoSem64Test,
            givenSemaphore64OffAndKernelWithWaitCbEventBelongingToCmdListWhenMutateIntoEventFromSameCmdListThenDataIsUpdatedAndNoopRemain) {
    waitCbEventBelongToCurrentMutateToCurrent<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderSem64Test,
            givenSemaphore64OnAndKernelWithWaitCbEventBelongingToCmdListWhenMutateIntoEventFromSameCmdListThenDataIsUpdatedAndNoopRemain) {
    waitCbEventBelongToCurrentMutateToCurrent<FamilyType>();
}

template <typename FamilyType>
void MutableCommandListFixtureInit::waitCbEventBelongToDifferentMutateToCurrent() {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t lriNoopSpace[sizeof(MI_LOAD_REGISTER_IMM)] = {0};
    alignas(uint32_t) uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = this->createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = this->createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto externalCmdList = this->createMutableCmdList();
    // attach event 1 to the external command list
    auto result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // attach event 2 to the command list
    result = this->mutableCommandList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = this->mutableCommandList->appendLaunchKernel(this->kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = this->getVariableList(this->commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    const size_t expectedLriSize = this->lriRequired ? 2 : 0;
    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;

    if (expectedLriSize > 0) {
        constexpr uint32_t firstRegister = 0x2600;
        constexpr uint32_t secondRegister = 0x2604;

        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);

        mutableLri = waitEventVar->getLoadRegImmList()[1];
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriUpperCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);

        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }

    result = this->mutableCommandList->updateMutableCommandWaitEvents(this->commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));

    if (expectedLriSize > 0) {
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderNoSem64Test,
            givenSemaphore64OffAndKernelWithWaitCbEventBelongingToDifferentCmdListWhenMutateIntoEventOwnedToCurrentCmdListThenDataIsUpdatedAndCommandNooped) {
    waitCbEventBelongToDifferentMutateToCurrent<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderSem64Test,
            givenSemaphore64OnAndKernelWithWaitCbEventBelongingToDifferentCmdListWhenMutateIntoEventOwnedToCurrentCmdListThenDataIsUpdatedAndCommandNooped) {
    waitCbEventBelongToDifferentMutateToCurrent<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenKernelWithWaitCbTimestampEventBelongingToDifferentCmdListWhenMutateIntoDifferentEventThenDataIsUpdated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto event = createTestEvent(true, false, true, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, true, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto externalCmdList = createMutableCmdList();
    // attach event 1 to the external command list
    auto result = externalCmdList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    // attach event 2 to the external command list
    result = externalCmdList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    uint64_t waitAddress = 0;
    if (mutableCommandList->getBase()->isInOrderCounterWaitRequired(event)) {
        waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    } else {
        waitAddress = event->getCompletionFieldGpuAddress(this->device);
    }
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    if (mutableCommandList->getBase()->isInOrderCounterWaitRequired(newEvent)) {
        waitAddress = newEvent->getInOrderExecEventHelper().getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
    } else {
        waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    }
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

template <typename FamilyType>
void MutableCommandListFixtureInit::waitCbEventBelongToDifferentNoopMutateBack() {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t lriNoopSpace[sizeof(MI_LOAD_REGISTER_IMM)] = {0};
    alignas(uint32_t) uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = this->createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = this->createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();
    ze_event_handle_t noopHandle = nullptr;

    auto externalCmdList = this->createMutableCmdList();
    // attach event 1 to the external command list
    auto result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = this->mutableCommandList->appendLaunchKernel(this->kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = this->getVariableList(this->commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    const size_t expectedLriSize = this->lriRequired ? 2 : 0;
    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;

    constexpr uint32_t firstRegister = 0x2600;
    constexpr uint32_t secondRegister = 0x2604;

    if (expectedLriSize > 0) {
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);

        mutableLri = waitEventVar->getLoadRegImmList()[1];
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriUpperCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);

        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }

    result = this->mutableCommandList->updateMutableCommandWaitEvents(this->commandId, 1, &noopHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));
    if (expectedLriSize > 0) {
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    result = this->mutableCommandList->updateMutableCommandWaitEvents(this->commandId, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
    if (expectedLriSize > 0) {
        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }

    result = this->mutableCommandList->updateMutableCommandWaitEvents(this->commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = newEvent->getInOrderExecEventHelper().getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    if (expectedLriSize > 0) {
        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderNoSem64Test,
            givenSemaphore64OffAndKernelWithWaitCbEventBelongingToDifferentCmdListWhenNoopedAndMutatedBackThenDataIsUpdatedAndCommandNoopedAndRestored) {
    waitCbEventBelongToDifferentNoopMutateBack<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderSem64Test,
            givenSemaphore64OnAndKernelWithWaitCbEventBelongingToDifferentCmdListWhenNoopedAndMutatedBackThenDataIsUpdatedAndCommandNoopedAndRestored) {
    waitCbEventBelongToDifferentNoopMutateBack<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenKernelWithWaitCbEventExternalBelongingToDifferentCmdListWhenAssigningCbEventToThirdCmdListAndMutateWaitEventThenPerformMutationCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    alignas(uint32_t) uint8_t noopSemWait[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    constexpr bool isExternalFlag = true;
    auto event = createTestEvent(true, false, false, false, isExternalFlag);
    auto eventHandle = event->toHandle();

    auto externalCmdList = createMutableCmdList();
    auto thirdCmdList = createMutableCmdList();
    // attach event 1 to the external command list
    auto result = externalCmdList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(2u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    EXPECT_EQ(0, memcmp(mockMutableSemWait->semWait, noopSemWait, sizeof(MI_SEMAPHORE_WAIT)));

    mutableSemWait = waitEventVar->getSemWaitList()[1];
    mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    // use cb event from third command list as wait event - will be attached to other in order exec info
    result = thirdCmdList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = thirdCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutate same event handle - should update the wait event to point to the new in order exec info
    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();

    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenKernelWithUnassignedWaitCbTimestampEventWhenMutatingIntoUnassignedCbTimestampEventThenNoopSpaceIsPreserved) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    alignas(uint32_t) uint8_t noopSemWait[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    constexpr bool isTimestamp = true;
    auto event = createTestEvent(true, false, isTimestamp, false, false);
    auto eventHandle = event->toHandle();

    auto newEvent = createTestEvent(true, false, isTimestamp, false, false);
    auto newEventHandle = newEvent->toHandle();

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    EXPECT_EQ(0, memcmp(mockMutableSemWait->semWait, noopSemWait, sizeof(MI_SEMAPHORE_WAIT)));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0, memcmp(mockMutableSemWait->semWait, noopSemWait, sizeof(MI_SEMAPHORE_WAIT)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenKernelWithUnassignedWaitCbTimestampEventWhenMutatingIntoAssignedCbTimestampEventThenNoopSpaceIsChangedIntoCommand) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    alignas(uint32_t) uint8_t noopSemWait[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    constexpr bool isTimestamp = true;
    auto event = createTestEvent(true, false, isTimestamp, false, false);
    auto eventHandle = event->toHandle();

    auto newEvent = createTestEvent(true, false, isTimestamp, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto externalCmdList = createMutableCmdList();
    auto result = externalCmdList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    EXPECT_EQ(0, memcmp(mockMutableSemWait->semWait, noopSemWait, sizeof(MI_SEMAPHORE_WAIT)));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(0, memcmp(mockMutableSemWait->semWait, noopSemWait, sizeof(MI_SEMAPHORE_WAIT)));

    auto semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    ASSERT_NE(nullptr, semWaitCmd);

    uint64_t waitAddress = 0;
    if (mutableCommandList->getBase()->isHeaplessModeEnabled()) {
        if (newEvent->isCbEventWithProfiling()) {
            waitAddress = newEvent->getGpuAddress(this->device);
        } else {
            auto &inOrderExecHelper = newEvent->getInOrderExecEventHelper();
            waitAddress = inOrderExecHelper.getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
        }
    } else {
        waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    }
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenKernelWithWaitRegularEventWhenWhenNoopedAndMutatedIntoDifferentEventThenDataIsUpdatedAndCommandNoopedAndRestoredIntoDifferentCompletion) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = createTestEvent(false, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();
    ze_event_handle_t noopHandle = nullptr;

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &noopHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_EQ(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

template <typename FamilyType>
void MutableCommandListFixtureInit::waitCbEventBelongToDifferentMutateToDifferent() {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto event = this->createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = this->createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto externalCmdList = this->createMutableCmdList();
    // attach both events to the external command list
    auto result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = this->mutableCommandList->appendLaunchKernel(this->kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = this->getVariableList(this->commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    const size_t expectedLriSize = this->lriRequired ? 2 : 0;
    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    auto waitValue = static_cast<uint32_t>(event->getInOrderExecEventHelper().getEventData()->counterValue);

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;

    if (expectedLriSize > 0) {
        constexpr uint32_t firstRegister = 0x2600;
        constexpr uint32_t secondRegister = 0x2604;

        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(getLowPart(waitValue), lriCmd->getDataDword());

        mutableLri = waitEventVar->getLoadRegImmList()[1];
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriUpperCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);

        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    } else {
        EXPECT_EQ(waitValue, static_cast<uint32_t>(NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd)));
    }

    // mutate to event 2
    result = this->mutableCommandList->updateMutableCommandWaitEvents(this->commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = newEvent->getInOrderExecEventHelper().getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    waitValue = static_cast<uint32_t>(newEvent->getInOrderExecEventHelper().getEventData()->counterValue);

    if (expectedLriSize > 0) {
        EXPECT_EQ(getLowPart(waitValue), lriCmd->getDataDword());
    } else {
        EXPECT_EQ(waitValue, static_cast<uint32_t>(NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd)));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderNoSem64Test,
            givenSemaphore64OffAndKernelWithWaitCbEventBelongingToDifferentCmdListWhenMutatedIntoCbEventBelongingToDifferentThenUpdateAddressAndValue) {
    waitCbEventBelongToDifferentMutateToDifferent<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderSem64Test,
            givenSemaphore64OnAndKernelWithWaitCbEventBelongingToDifferentCmdListWhenMutatedIntoCbEventBelongingToDifferentThenUpdateAddressAndValue) {
    waitCbEventBelongToDifferentMutateToDifferent<FamilyType>();
}

template <typename FamilyType>
void MutableCommandListFixtureInit::mutableWaitEventsOnAppendOperations(
    MutableEventOnAppendOperationCallback callbackInit,
    bool doNotSelectWaitEvents,
    bool createCbEvent,
    bool doNotGetNextCommandId) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto event = this->createTestEvent(createCbEvent, false, false, false, false);
    auto eventHandle = event->toHandle();

    MutableWaitEventsOnAppendOperationsData callbackData = {};

    if (createCbEvent) {
        callbackData.signalEvent = eventHandle;
    } else {
        callbackData.waitEvents = &eventHandle;
        callbackData.numWaitEvents = 1;
    }

    if (doNotSelectWaitEvents) {
        this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_SIGNAL_EVENT;
    } else {
        this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    if (doNotGetNextCommandId == false) {
        result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }

    // create resources and call the append
    (this->*callbackInit)(&callbackData);
    if (createCbEvent) {
        if (callbackData.cbEventAsWaitEvent) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, callbackData.result);
        } else {
            EXPECT_NE(ZE_RESULT_SUCCESS, callbackData.result);
        }
    } else {
        EXPECT_EQ(ZE_RESULT_SUCCESS, callbackData.result);

        if (doNotSelectWaitEvents || doNotGetNextCommandId) {
            EXPECT_EQ(nullptr, callbackData.outWaitCmds);
        } else {
            EXPECT_NE(nullptr, callbackData.outWaitCmds);
            EXPECT_TRUE(callbackData.skipAddingWaitEventsToResidency);
        }
    }

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    if (doNotGetNextCommandId) {
        EXPECT_EQ(0u, mutableCommandList->eventMutations.size());
    } else {
        auto waitEvents = this->getVariableList(this->commandId, L0::MCL::VariableType::waitEvent, nullptr);
        if (doNotSelectWaitEvents || createCbEvent) {
            EXPECT_EQ(0u, waitEvents.size());
        } else {
            ASSERT_EQ(1u, waitEvents.size());
            auto waitEventVar = waitEvents[0];
            auto mutableSemWait = waitEventVar->getSemWaitList()[0];
            auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
            auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
            auto waitAddress = event->getGpuAddress(this->device);

            EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
        }
    }

    if (callbackData.srcImageHandle) {
        auto srcImage = L0::Image::fromHandle(callbackData.srcImageHandle);
        srcImage->destroy();
    }
    if (callbackData.dstImageHandle) {
        auto dstImage = L0::Image::fromHandle(callbackData.dstImageHandle);
        dstImage->destroy();
    }

    result = this->mutableCommandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendBarrierThenExpectCreateWaitEventVariable) {

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendBarrierCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendBarrierCallback, true, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendBarrierCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendBarrierCallback, false, false, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryRangesBarrierThenExpectCreateWaitEventVariable) {

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendRangesBarrierCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendRangesBarrierCallback, true, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendRangesBarrierCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendRangesBarrierCallback, false, false, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryCopyThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::copyBufferToBufferMiddle, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryCopyRegionThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::copyBufferToBufferMiddle, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyRegionCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyRegionCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyRegionCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryCopyWithParametersThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::copyBufferToBufferMiddle, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyWithParametersCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyWithParametersCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyWithParametersCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryCopyFromContextThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::copyBufferToBufferMiddle, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyFromContextCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyFromContextCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyFromContextCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryFillThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::fillBufferImmediate, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryFillWithParametersThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::fillBufferImmediate, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillWithParametersCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillWithParametersCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillWithParametersCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryImageCopyFromMemoryThenExpectCreateWaitEventVariable) {
    if constexpr (FamilyType::supportsSampler) {
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltIn::copyImageRegion, getDefaultBuiltInMode());
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;

        auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
        auto isa = kernelImmutableData->getIsaGraphicsAllocation();
        isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryCallback,
                                                        false, false, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryCallback,
                                                        false, true, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryCallback,
                                                        false, false, true);

        isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryImageCopyFromMemoryExtThenExpectCreateWaitEventVariable) {
    if constexpr (FamilyType::supportsSampler) {
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltIn::copyImageRegion, getDefaultBuiltInMode());
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;

        auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
        auto isa = kernelImmutableData->getIsaGraphicsAllocation();
        isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryExtCallback,
                                                        false, false, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryExtCallback,
                                                        false, true, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryExtCallback,
                                                        false, false, true);

        isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryImageCopyToMemoryThenExpectCreateWaitEventVariable) {
    if constexpr (FamilyType::supportsSampler) {
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltIn::copyImageRegion, getDefaultBuiltInMode());
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;

        auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
        auto isa = kernelImmutableData->getIsaGraphicsAllocation();
        isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryCallback,
                                                        false, false, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryCallback,
                                                        false, true, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryCallback,
                                                        false, false, true);

        isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryImageCopyToMemoryExtThenExpectCreateWaitEventVariable) {
    if constexpr (FamilyType::supportsSampler) {
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltIn::copyImageRegion, getDefaultBuiltInMode());
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;

        auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
        auto isa = kernelImmutableData->getIsaGraphicsAllocation();
        isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryExtCallback,
                                                        false, false, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryExtCallback,
                                                        false, true, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryExtCallback,
                                                        false, false, true);

        isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryImageCopyThenExpectCreateWaitEventVariable) {
    if constexpr (FamilyType::supportsSampler) {
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltIn::copyImageRegion, getDefaultBuiltInMode());
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;

        auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
        auto isa = kernelImmutableData->getIsaGraphicsAllocation();
        isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyCallback,
                                                        false, false, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyCallback,
                                                        false, true, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyCallback,
                                                        false, false, true);

        isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryImageCopyRegionThenExpectCreateWaitEventVariable) {
    if constexpr (FamilyType::supportsSampler) {
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltIn::copyImageRegion, getDefaultBuiltInMode());
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;

        auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
        auto isa = kernelImmutableData->getIsaGraphicsAllocation();
        isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyRegionCallback,
                                                        false, false, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyRegionCallback,
                                                        false, true, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyRegionCallback,
                                                        false, false, true);

        isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendWaitOnEventsThenExpectCreateWaitEventVariable) {
    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWaitOnEventsCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWaitOnEventsCallback, true, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWaitOnEventsCallback, true, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWaitOnEventsCallback, false, false, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendWriteGlobalTimestampThenExpectCreateWaitEventVariable) {
    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWriteGlobalTimestampCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWriteGlobalTimestampCallback, true, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWriteGlobalTimestampCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWriteGlobalTimestampCallback, false, false, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendQueryKernelTimestampsThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::queryKernelTimestamps, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendQueryKernelTimestampsCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendQueryKernelTimestampsCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendHostFunctionThenExpectCreateWaitEventVariable) {
    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendHostFunctionCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendHostFunctionCallback, true, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendHostFunctionCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendHostFunctionCallback, false, false, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenExternalCbEventWithPatchPreambleWhenAppendingKernelWithEventAndMutatingThenNewPatchPreambleWaitIsSet) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MI_SEMAPHORE_WAIT templateSemWait;

    alignas(uint32_t) uint8_t noopSemWait[sizeof(MI_SEMAPHORE_WAIT)] = {0};
    alignas(uint32_t) uint8_t noopLri[sizeof(MI_LOAD_REGISTER_IMM)] = {0};

    auto event = createTestEvent(true, false, false, false, true);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, false, false, true);
    auto newEventHandle = newEvent->toHandle();
    ze_event_handle_t noopHandle = nullptr;

    auto otherCmdlist = createMutableCmdList();
    // attach wait events to other command list
    L0::CmdListWaitEventParameters waitEventParams = {};
    L0::CmdListSignalEventParameters signalEventParams = {};
    otherCmdlist->appendBarrier(eventHandle, 0, nullptr, waitEventParams, signalEventParams);
    otherCmdlist->appendBarrier(newEventHandle, 0, nullptr, waitEventParams, signalEventParams);
    otherCmdlist->close();

    // assign them counters
    uint64_t counter = 0x123;
    auto counterLow = getLowPart(counter);
    uint64_t deviceGpuAddress = 0xAB000;
    MockGraphicsAllocation counterAllocation(nullptr, deviceGpuAddress, sizeof(uint64_t));
    event->getInOrderExecEventHelper().assignPatchPreambleData(counter, nullptr, 0, nullptr, deviceGpuAddress, &counterAllocation);

    constexpr bool registerPollMode = false;
    constexpr bool waitMode = true;
    constexpr bool switchOnUnsuccessful = false;

    auto waitValue = this->lriRequired ? 0 : counter;
    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&templateSemWait,
                                                             deviceGpuAddress,
                                                             waitValue,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
                                                             registerPollMode, waitMode, this->qwordInUse, this->lriRequired, switchOnUnsuccessful, this->sem64bSupport);

    uint64_t newCounter = 0x456;
    auto newCounterLow = getLowPart(newCounter);
    uint64_t newDeviceGpuAddress = 0xCD000;
    MockGraphicsAllocation newCounterAllocation(nullptr, newDeviceGpuAddress, sizeof(uint64_t));
    newEvent->getInOrderExecEventHelper().assignPatchPreambleData(newCounter, nullptr, 0, nullptr, newDeviceGpuAddress, &newCounterAllocation);

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(isAllocationInMutableResidency(mutableCommandList.get(), &counterAllocation));

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(2u, waitEventVar->getSemWaitList().size());

    // patch preamble mutable sem wait
    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    ASSERT_NE(nullptr, semWaitCmd);
    EXPECT_EQ(deviceGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
    EXPECT_EQ(0, memcmp(&templateSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    if (this->lriRequired) {
        ASSERT_EQ(4u, waitEventVar->getLoadRegImmList().size());
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        ASSERT_NE(nullptr, lriCmd);
        EXPECT_EQ(counterLow, lriCmd->getDataDword());
    } else {
        EXPECT_EQ(counter, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
    }

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &noopHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(isAllocationInMutableResidency(mutableCommandList.get(), &counterAllocation));

    EXPECT_EQ(0, memcmp(semWaitCmd, noopSemWait, sizeof(MI_SEMAPHORE_WAIT)));
    if (this->lriRequired) {
        EXPECT_EQ(0, memcmp(lriCmd, noopLri, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(isAllocationInMutableResidency(mutableCommandList.get(), &newCounterAllocation));

    semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitCmd);
    ASSERT_NE(nullptr, semWaitCmd);
    EXPECT_EQ(newDeviceGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    if (this->lriRequired) {
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriCmd);
        ASSERT_NE(nullptr, lriCmd);
        EXPECT_EQ(newCounterLow, lriCmd->getDataDword());
    } else {
        EXPECT_EQ(newCounter, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
    }

    waitValue = this->lriRequired ? 0 : newCounter;
    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&templateSemWait,
                                                             newDeviceGpuAddress,
                                                             waitValue,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
                                                             registerPollMode, waitMode, this->qwordInUse, this->lriRequired, switchOnUnsuccessful, this->sem64bSupport);
    EXPECT_EQ(0, memcmp(&templateSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenExternalCbEventWithPatchPreambleCounterExceeding32BitBoundryWhenAppendingKernelWithWaitEventAndMutatingThenNewPatchPreambleWaitIsSetAndDeviceCounterUsesCorrectValues) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MI_SEMAPHORE_WAIT templateSemWait;

    auto event = createTestEvent(true, false, false, false, true);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, false, false, true);
    auto newEventHandle = newEvent->toHandle();

    auto otherCmdlist = createMutableCmdList();
    // attach wait events to other command list
    L0::CmdListWaitEventParameters waitEventParams = {};
    L0::CmdListSignalEventParameters signalEventParams = {};
    otherCmdlist->appendBarrier(eventHandle, 0, nullptr, waitEventParams, signalEventParams);
    otherCmdlist->appendBarrier(newEventHandle, 0, nullptr, waitEventParams, signalEventParams);
    otherCmdlist->close();

    // assign them counters
    uint32_t counterLow = 2;
    uint64_t counter = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1 + counterLow;
    auto lriCounterLow = getLowPart(counter);
    auto lriCounterHigh = getHighPart(counter);
    uint64_t deviceGpuAddress = 0xAB000;
    MockGraphicsAllocation counterAllocation(nullptr, deviceGpuAddress, sizeof(uint64_t));
    event->getInOrderExecEventHelper().assignPatchPreambleData(counter, nullptr, 0, nullptr, deviceGpuAddress, &counterAllocation);

    constexpr bool registerPollMode = false;
    constexpr bool waitMode = true;
    constexpr bool switchOnUnsuccessful = false;

    // when qword not in use, then use lower 32b, when in use, check if lri is required, if so, use 0, else use full 64b counter
    auto waitValue = this->qwordInUse == false ? counterLow : this->lriRequired ? 0
                                                                                : counter;
    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&templateSemWait,
                                                             deviceGpuAddress,
                                                             waitValue,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
                                                             registerPollMode, waitMode, this->qwordInUse, this->lriRequired, switchOnUnsuccessful, this->sem64bSupport);

    uint32_t newCounterLow = 4;
    uint64_t newCounter = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1 + newCounterLow;
    auto newLriCounterLow = getLowPart(newCounter);
    auto newLriCounterHigh = getHighPart(newCounter);
    uint64_t newDeviceGpuAddress = 0xCD000;
    MockGraphicsAllocation newCounterAllocation(nullptr, newDeviceGpuAddress, sizeof(uint64_t));
    newEvent->getInOrderExecEventHelper().assignPatchPreambleData(newCounter, nullptr, 0, nullptr, newDeviceGpuAddress, &newCounterAllocation);

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(isAllocationInMutableResidency(mutableCommandList.get(), &counterAllocation));

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(2u, waitEventVar->getSemWaitList().size());

    // patch preamble mutable sem wait
    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    ASSERT_NE(nullptr, semWaitCmd);
    EXPECT_EQ(deviceGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
    EXPECT_EQ(0, memcmp(&templateSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    if (this->lriRequired) {
        ASSERT_EQ(4u, waitEventVar->getLoadRegImmList().size());
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        ASSERT_NE(nullptr, lriCmd);
        EXPECT_EQ(lriCounterLow, lriCmd->getDataDword());

        auto mutableLriHigh = waitEventVar->getLoadRegImmList()[1];
        auto mockMutableLriHigh = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLriHigh);
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLriHigh->loadRegImm);
        ASSERT_NE(nullptr, lriCmd);
        EXPECT_EQ(lriCounterHigh, lriCmd->getDataDword());

        EXPECT_EQ(0u, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
    } else {
        if (this->qwordInUse) {
            EXPECT_EQ(counter, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
        } else {
            EXPECT_EQ(counterLow, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
        }
    }

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(isAllocationInMutableResidency(mutableCommandList.get(), &newCounterAllocation));

    semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitCmd);
    ASSERT_NE(nullptr, semWaitCmd);
    EXPECT_EQ(newDeviceGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    if (this->lriRequired) {
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        ASSERT_NE(nullptr, lriCmd);
        EXPECT_EQ(newLriCounterLow, lriCmd->getDataDword());

        auto mutableLriHigh = waitEventVar->getLoadRegImmList()[1];
        auto mockMutableLriHigh = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLriHigh);
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLriHigh->loadRegImm);
        ASSERT_NE(nullptr, lriCmd);
        EXPECT_EQ(newLriCounterHigh, lriCmd->getDataDword());

        EXPECT_EQ(0u, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
    } else {
        if (this->qwordInUse) {
            EXPECT_EQ(newCounter, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
        } else {
            EXPECT_EQ(newCounterLow, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
        }
    }

    waitValue = this->qwordInUse == false ? newCounterLow : this->lriRequired ? 0
                                                                              : newCounter;
    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&templateSemWait,
                                                             newDeviceGpuAddress,
                                                             waitValue,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
                                                             registerPollMode, waitMode, this->qwordInUse, this->lriRequired, switchOnUnsuccessful, this->sem64bSupport);
    EXPECT_EQ(0, memcmp(&templateSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenGraphInstantiationTargetWhenSwitchingCounterBasedEventThenRestrictionIsTracked) {
    auto *event = createTestEvent(true, true, false, false, false);
    ASSERT_TRUE(event->isCounterBasedExplicitlyEnabled());
    ASSERT_FALSE(event->isExternalEvent());
    ASSERT_FALSE(event->getIsSignalledAsGraphInternalEvent());

    mutableCommandList->getBase()->setIsGraphInstantiationTarget(true);
    mutableCommandList->switchCounterBasedEvents(0, 0, event, false);
    EXPECT_TRUE(event->getIsSignalledAsGraphInternalEvent());

    mutableCommandList->getBase()->setIsGraphInstantiationTarget(false);
    mutableCommandList->switchCounterBasedEvents(0, 0, event, false);
    EXPECT_FALSE(event->getIsSignalledAsGraphInternalEvent());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenGraphInstantiationTargetWhenSwitchingExternalCounterBasedEventThenRestrictionIsNotApplied) {
    auto *event = createTestEvent(true, true, false, false, true);
    ASSERT_TRUE(event->isExternalEvent());

    mutableCommandList->getBase()->setIsGraphInstantiationTarget(true);
    mutableCommandList->switchCounterBasedEvents(0, 0, event, false);
    EXPECT_FALSE(event->getIsSignalledAsGraphInternalEvent());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenUnassignedCbEventWhenAppendingKernelWithUnassignedEventAndMutatingIntoAssignedThenCommandIsNoopedAndAfterMutationRestored) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MI_SEMAPHORE_WAIT noopSemWait;
    memset(&noopSemWait, 0, sizeof(MI_SEMAPHORE_WAIT));

    MI_LOAD_REGISTER_IMM noopLri;
    memset(&noopLri, 0, sizeof(MI_LOAD_REGISTER_IMM));

    auto event = createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto otherCmdlist = createMutableCmdList();
    // attach new wait event to other command list
    L0::CmdListWaitEventParameters waitEventParams = {};
    L0::CmdListSignalEventParameters signalEventParams = {};
    otherCmdlist->appendBarrier(newEventHandle, 0, nullptr, waitEventParams, signalEventParams);
    otherCmdlist->close();

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];

    EXPECT_TRUE(waitEventVar->getDesc().eventValue.noopState);

    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    // sem wait is nooped
    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    MI_SEMAPHORE_WAIT *semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    EXPECT_EQ(0, memcmp(&noopSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriHighCmd = nullptr;
    if (this->lriRequired) {
        ASSERT_EQ(2u, waitEventVar->getLoadRegImmList().size());
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(&noopLri, lriCmd, sizeof(MI_LOAD_REGISTER_IMM)));

        auto mutableLriHigh = waitEventVar->getLoadRegImmList()[1];
        auto mockMutableLriHigh = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLriHigh);
        lriHighCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLriHigh->loadRegImm);
        EXPECT_EQ(0, memcmp(&noopLri, lriHighCmd, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(waitEventVar->getDesc().eventValue.noopState);

    semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitCmd);
    ASSERT_NE(nullptr, semWaitCmd);

    auto waitAddress = newEvent->getInOrderExecEventHelper().getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    if (this->lriRequired) {
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriCmd);
        ASSERT_NE(nullptr, lriCmd);

        lriHighCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriHighCmd);
        ASSERT_NE(nullptr, lriHighCmd);
    }

    // mutate back into noop state
    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(waitEventVar->getDesc().eventValue.noopState);

    EXPECT_EQ(0, memcmp(&noopSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));
    if (this->lriRequired) {
        EXPECT_EQ(0, memcmp(&noopLri, lriCmd, sizeof(MI_LOAD_REGISTER_IMM)));
        EXPECT_EQ(0, memcmp(&noopLri, lriHighCmd, sizeof(MI_LOAD_REGISTER_IMM)));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenAssignedCbEventWhenAppendingKernelWithAssignedEventAndMutatingIntoUnassignedThenCommandIsProgramedAndAfterMutationNooped) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MI_SEMAPHORE_WAIT noopSemWait;
    memset(&noopSemWait, 0, sizeof(MI_SEMAPHORE_WAIT));

    MI_LOAD_REGISTER_IMM noopLri;
    memset(&noopLri, 0, sizeof(MI_LOAD_REGISTER_IMM));

    auto event = createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto otherCmdlist = createMutableCmdList();
    // attach appending wait event to other command list
    L0::CmdListWaitEventParameters waitEventParams = {};
    L0::CmdListSignalEventParameters signalEventParams = {};
    otherCmdlist->appendBarrier(eventHandle, 0, nullptr, waitEventParams, signalEventParams);
    otherCmdlist->close();

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];

    EXPECT_FALSE(waitEventVar->getDesc().eventValue.noopState);

    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    // sem wait is nooped
    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    MI_SEMAPHORE_WAIT *semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    ASSERT_NE(nullptr, semWaitCmd);

    auto waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriHighCmd = nullptr;
    if (this->lriRequired) {
        ASSERT_EQ(2u, waitEventVar->getLoadRegImmList().size());
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        ASSERT_NE(nullptr, lriCmd);

        auto mutableLriHigh = waitEventVar->getLoadRegImmList()[1];
        auto mockMutableLriHigh = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLriHigh);
        lriHighCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLriHigh->loadRegImm);
        ASSERT_NE(nullptr, lriHighCmd);
    }

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(waitEventVar->getDesc().eventValue.noopState);

    EXPECT_EQ(0, memcmp(&noopSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));
    if (this->lriRequired) {
        EXPECT_EQ(0, memcmp(&noopLri, lriCmd, sizeof(MI_LOAD_REGISTER_IMM)));
        EXPECT_EQ(0, memcmp(&noopLri, lriHighCmd, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    // mutate back into noop state
    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(waitEventVar->getDesc().eventValue.noopState);

    semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitCmd);
    ASSERT_NE(nullptr, semWaitCmd);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    if (this->lriRequired) {
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriCmd);
        ASSERT_NE(nullptr, lriCmd);
        lriHighCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriHighCmd);
        ASSERT_NE(nullptr, lriHighCmd);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenUnassignedExternalCbEventAndAppendingKernelWithUnassignedEventWhenAssigningExternalAndMutationRefreshThenCommandIsNoopedAndAfterMutationRestored) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MI_SEMAPHORE_WAIT noopSemWait;
    memset(&noopSemWait, 0, sizeof(MI_SEMAPHORE_WAIT));

    MI_LOAD_REGISTER_IMM noopLri;
    memset(&noopLri, 0, sizeof(MI_LOAD_REGISTER_IMM));

    auto event = createTestEvent(true, false, false, false, true);
    auto eventHandle = event->toHandle();

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];

    EXPECT_TRUE(waitEventVar->getDesc().eventValue.noopState);
    EXPECT_TRUE(waitEventVar->getDesc().eventValue.patchPreambleNoopState);

    ASSERT_EQ(2u, waitEventVar->getSemWaitList().size());
    MI_SEMAPHORE_WAIT *semWaitCmdPatchPreamble = nullptr;
    MI_SEMAPHORE_WAIT *semWaitCmdEvent = nullptr;
    {
        auto mutableSemWait = waitEventVar->getSemWaitList()[0];
        auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
        semWaitCmdPatchPreamble = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
        EXPECT_EQ(0, memcmp(&noopSemWait, semWaitCmdPatchPreamble, sizeof(MI_SEMAPHORE_WAIT)));
    }
    {
        auto mutableSemWait = waitEventVar->getSemWaitList()[1];
        auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
        semWaitCmdEvent = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
        EXPECT_EQ(0, memcmp(&noopSemWait, semWaitCmdEvent, sizeof(MI_SEMAPHORE_WAIT)));
    }

    MI_LOAD_REGISTER_IMM *lriCmdPatchPreamble = nullptr;
    MI_LOAD_REGISTER_IMM *lriHighCmdPatchPreamble = nullptr;

    MI_LOAD_REGISTER_IMM *lriCmdEvent = nullptr;
    MI_LOAD_REGISTER_IMM *lriHighCmdEvent = nullptr;

    if (this->lriRequired) {
        ASSERT_EQ(4u, waitEventVar->getLoadRegImmList().size());
        {
            auto mutableLri = waitEventVar->getLoadRegImmList()[0];
            auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
            lriCmdPatchPreamble = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
            EXPECT_EQ(0, memcmp(&noopLri, lriCmdPatchPreamble, sizeof(MI_LOAD_REGISTER_IMM)));
        }
        {
            auto mutableLri = waitEventVar->getLoadRegImmList()[1];
            auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
            lriHighCmdPatchPreamble = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
            EXPECT_EQ(0, memcmp(&noopLri, lriHighCmdPatchPreamble, sizeof(MI_LOAD_REGISTER_IMM)));
        }
        {
            auto mutableLri = waitEventVar->getLoadRegImmList()[2];
            auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
            lriCmdEvent = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
            EXPECT_EQ(0, memcmp(&noopLri, lriCmdEvent, sizeof(MI_LOAD_REGISTER_IMM)));
        }
        {
            auto mutableLri = waitEventVar->getLoadRegImmList()[2];
            auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
            lriHighCmdEvent = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
            EXPECT_EQ(0, memcmp(&noopLri, lriHighCmdEvent, sizeof(MI_LOAD_REGISTER_IMM)));
        }
    }

    auto otherCmdlist = createMutableCmdList();
    // attach new wait event to other command list
    L0::CmdListWaitEventParameters waitEventParams = {};
    L0::CmdListSignalEventParameters signalEventParams = {};
    otherCmdlist->appendBarrier(eventHandle, 0, nullptr, waitEventParams, signalEventParams);
    otherCmdlist->close();

    uint64_t patchPreambleCounter = 4;
    uint64_t patchPreambleDeviceGpuAddress = 0xCD000;
    MockGraphicsAllocation patchPreambleDeviceAllocation(nullptr, patchPreambleDeviceGpuAddress, sizeof(uint64_t));
    event->getInOrderExecEventHelper().assignPatchPreambleData(patchPreambleCounter, nullptr, 0, nullptr, patchPreambleDeviceGpuAddress, &patchPreambleDeviceAllocation);

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(waitEventVar->getDesc().eventValue.noopState);
    EXPECT_FALSE(waitEventVar->getDesc().eventValue.patchPreambleNoopState);

    semWaitCmdPatchPreamble = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitCmdPatchPreamble);
    EXPECT_EQ(patchPreambleDeviceGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmdPatchPreamble));

    semWaitCmdEvent = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitCmdEvent);
    ASSERT_NE(nullptr, semWaitCmdEvent);

    auto waitAddressEvent = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddressEvent, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmdEvent));

    if (this->lriRequired) {
        lriCmdPatchPreamble = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriCmdPatchPreamble);
        ASSERT_NE(nullptr, lriCmdPatchPreamble);

        lriHighCmdPatchPreamble = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriHighCmdPatchPreamble);
        ASSERT_NE(nullptr, lriHighCmdPatchPreamble);

        lriCmdEvent = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriCmdEvent);
        ASSERT_NE(nullptr, lriCmdEvent);

        lriHighCmdEvent = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriHighCmdEvent);
        ASSERT_NE(nullptr, lriHighCmdEvent);
    }
}

struct MutableCommandListPatchPreambleWaitFixture : public MutableCommandListFixture<true, -1> {
    void setUp() {
        MutableCommandListFixture<true, -1>::setUp();
        signalCmdList = createMutableCmdList();
    }

    void tearDown() {
        signalCmdList.reset();
        MutableCommandListFixture<true, -1>::tearDown();
    }

    void appendSignal(MutableCommandList *cmdList, Event *event) {
        L0::CmdListWaitEventParameters waitEventParams = {};
        L0::CmdListSignalEventParameters signalEventParams = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList->appendBarrier(event->toHandle(), 0, nullptr, waitEventParams, signalEventParams));
    }

    bool captureWaitOnSignaledEvent(Event *event) {
        appendSignal(signalCmdList.get(), event);

        mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
        EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));
        auto eventHandle = event->toHandle();
        EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams));
        EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

        auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
        if (waitEvents.size() != 1u) {
            return false;
        }
        waitEventVar = waitEvents[0];

        // wait on timestamp event signaled before capture is captured in regular shape with patch preamble wait ahead of it
        auto &semWaits = waitEventVar->getSemWaitList();
        return semWaits.size() == 2u &&
               semWaits[patchPreambleSemWaitIndex]->getType() == L0::MCL::MutableSemaphoreWait::cbEventWaitPatchPreambleCounter &&
               semWaits[eventSemWaitIndex]->getType() == L0::MCL::MutableSemaphoreWait::regularEventWait;
    }

    void mutateWaitEvent(Event *event) {
        auto eventHandle = event->toHandle();
        EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &eventHandle));
        EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());
    }

    void assignPatchPreambleCounter(Event *event) {
        event->getInOrderExecEventHelper().assignPatchPreambleData(patchPreambleCounter, nullptr, 0, nullptr, patchPreambleAllocation.getGpuAddress(), &patchPreambleAllocation);
    }

    template <typename FamilyType>
    typename FamilyType::MI_SEMAPHORE_WAIT *getSemWait(size_t index) {
        return reinterpret_cast<typename FamilyType::MI_SEMAPHORE_WAIT *>(static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(waitEventVar->getSemWaitList()[index])->semWait);
    }

    template <typename FamilyType>
    bool isSemWaitNooped(size_t index) {
        return memoryZeroed(getSemWait<FamilyType>(index), sizeof(typename FamilyType::MI_SEMAPHORE_WAIT));
    }

    template <typename FamilyType>
    void expectPatchPreambleWaitNooped() {
        EXPECT_TRUE(waitEventVar->getDesc().eventValue.patchPreambleNoopState);
        EXPECT_TRUE(isSemWaitNooped<FamilyType>(patchPreambleSemWaitIndex));
    }

    template <typename FamilyType>
    void expectPatchPreambleWaitProgrammed() {
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

        EXPECT_FALSE(waitEventVar->getDesc().eventValue.patchPreambleNoopState);
        auto semWait = genCmdCast<MI_SEMAPHORE_WAIT *>(getSemWait<FamilyType>(patchPreambleSemWaitIndex));
        ASSERT_NE(nullptr, semWait);
        EXPECT_EQ(patchPreambleAllocation.getGpuAddress(), NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWait));
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, semWait->getCompareOperation());
        if (!waitEventVar->getDesc().eventValue.qwordIndirect) {
            EXPECT_EQ(patchPreambleCounter, semWait->getSemaphoreDataDword());
        }
    }

    template <typename FamilyType>
    void expectEventWaitNooped() {
        EXPECT_TRUE(waitEventVar->getDesc().eventValue.noopState);
        EXPECT_TRUE(isSemWaitNooped<FamilyType>(eventSemWaitIndex));
    }

    template <typename FamilyType>
    void expectEventWaitProgrammed(Event *event) {
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

        EXPECT_FALSE(waitEventVar->getDesc().eventValue.noopState);
        auto semWait = genCmdCast<MI_SEMAPHORE_WAIT *>(getSemWait<FamilyType>(eventSemWaitIndex));
        ASSERT_NE(nullptr, semWait);
        EXPECT_EQ(event->getCompletionFieldGpuAddress(this->device), NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWait));
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semWait->getCompareOperation());
    }

    static constexpr size_t patchPreambleSemWaitIndex = 0;
    static constexpr size_t eventSemWaitIndex = 1;
    static constexpr uint64_t patchPreambleCounter = 4;

    MockGraphicsAllocation patchPreambleAllocation{nullptr, 0xCD000, sizeof(uint64_t)};
    std::unique_ptr<MutableCommandList> signalCmdList;
    L0::MCL::Variable *waitEventVar = nullptr;
};

using MutableCommandListPatchPreambleWaitTest = Test<MutableCommandListPatchPreambleWaitFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListPatchPreambleWaitTest,
            givenExternalCbTimestampEventSignaledBeforeCaptureWhenRefreshingThenEventWaitIsUpdatedAndPatchPreambleWaitFollowsPatchPreambleCounter) {
    if (mutableCommandList->getBase()->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    auto event = createTestEvent(true, false, true, false, true);
    ASSERT_TRUE(captureWaitOnSignaledEvent(event));
    expectPatchPreambleWaitNooped<FamilyType>();

    appendSignal(signalCmdList.get(), event);
    mutateWaitEvent(event);
    expectPatchPreambleWaitNooped<FamilyType>();
    expectEventWaitProgrammed<FamilyType>(event);

    assignPatchPreambleCounter(event);
    mutateWaitEvent(event);
    expectPatchPreambleWaitProgrammed<FamilyType>();
    expectEventWaitProgrammed<FamilyType>(event);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListPatchPreambleWaitTest,
            givenNoopedWaitOnExternalCbTimestampEventSignaledBeforeCaptureWhenMutatingBackThenEventWaitIsRestoredAndPatchPreambleWaitFollowsPatchPreambleCounter) {
    if (mutableCommandList->getBase()->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    auto event = createTestEvent(true, false, true, false, true);
    auto unsignaledEvent = createTestEvent(true, false, true, false, true);
    ASSERT_TRUE(captureWaitOnSignaledEvent(event));

    mutateWaitEvent(unsignaledEvent);
    expectPatchPreambleWaitNooped<FamilyType>();
    expectEventWaitNooped<FamilyType>();

    mutateWaitEvent(event);
    expectPatchPreambleWaitNooped<FamilyType>();
    expectEventWaitProgrammed<FamilyType>(event);

    mutateWaitEvent(unsignaledEvent);
    expectEventWaitNooped<FamilyType>();

    assignPatchPreambleCounter(event);
    mutateWaitEvent(event);
    expectPatchPreambleWaitProgrammed<FamilyType>();
    expectEventWaitProgrammed<FamilyType>(event);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListPatchPreambleWaitTest,
            givenWaitOnExternalCbTimestampEventSignaledBeforeCaptureWhenMutatingIntoTimestampEventBoundToCmdListThenBothWaitsAreNoopedOnRegularPath) {
    if (mutableCommandList->getBase()->isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    auto event = createTestEvent(true, false, true, false, true);
    auto boundEvent = createTestEvent(true, false, true, false, true);
    appendSignal(mutableCommandList.get(), boundEvent);
    ASSERT_TRUE(boundEvent->hasInOrderTimestampNode());
    assignPatchPreambleCounter(boundEvent);

    ASSERT_TRUE(captureWaitOnSignaledEvent(event));

    mutateWaitEvent(boundEvent);
    expectEventWaitNooped<FamilyType>();
    // patch preamble noop state follows the new patch preamble counter, the command itself is nooped together with the event wait
    EXPECT_FALSE(waitEventVar->getDesc().eventValue.patchPreambleNoopState);
    EXPECT_TRUE(isSemWaitNooped<FamilyType>(patchPreambleSemWaitIndex));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenOwningCommandViewVariantsInAppendCmdsToPatchWhenClearMutableAppendDataThenCommandViewsAreFreedAndContainerCleared) {
    auto whiteBoxCmdList = static_cast<MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> *>(mutableCommandList->base);
    const bool semaphore64bCmdSupported = whiteBoxCmdList->semaphore64bCmdSupported;

    // Seed appendCmdsToPatch with all six variants that own an allocated commandView,
    // allocating each view the same way the encode helpers do internally
    // (allocate*Command <-> deallocate*Command pairs used by clearMutableAppendData()).
    {
        PatchCbWaitEventLoadRegisterImm lri{};
        lri.commandView = NEO::EncodeSetMMIO<FamilyType>::allocateLoadRegisterImmCommand();
        whiteBoxCmdList->appendCmdsToPatch.push_back(lri);

        PatchExternalCbWaitEventPreambleCounterLoadRegisterImm externalLri{};
        externalLri.commandView = NEO::EncodeSetMMIO<FamilyType>::allocateLoadRegisterImmCommand();
        whiteBoxCmdList->appendCmdsToPatch.push_back(externalLri);

        PatchCbWaitEventSemaphoreWait cbEventSemWait{};
        cbEventSemWait.commandView = NEO::EncodeSemaphore<FamilyType>::allocateSemaphoreWaitCommand(semaphore64bCmdSupported);
        whiteBoxCmdList->appendCmdsToPatch.push_back(cbEventSemWait);

        PatchExternalCbWaitEventPreambleCounterSemaphoreWait externalSemWait{};
        externalSemWait.commandView = NEO::EncodeSemaphore<FamilyType>::allocateSemaphoreWaitCommand(semaphore64bCmdSupported);
        whiteBoxCmdList->appendCmdsToPatch.push_back(externalSemWait);

        PatchWaitEventSemaphoreWait semWait{};
        semWait.commandView = NEO::EncodeSemaphore<FamilyType>::allocateSemaphoreWaitCommand(semaphore64bCmdSupported);
        whiteBoxCmdList->appendCmdsToPatch.push_back(semWait);

        PatchCbEventTimestampPostSyncSemaphoreWait timestampSemWait{};
        timestampSemWait.commandView = NEO::EncodeSemaphore<FamilyType>::allocateSemaphoreWaitCommand(semaphore64bCmdSupported);
        whiteBoxCmdList->appendCmdsToPatch.push_back(timestampSemWait);
    }

    // Add a non-owning variant to verify the lambda skips it and does not crash.
    {
        PatchCbEventTimestampClearStoreDataImm timestampClearStore{};
        whiteBoxCmdList->appendCmdsToPatch.push_back(timestampClearStore);
    }

    ASSERT_EQ(7u, whiteBoxCmdList->appendCmdsToPatch.size());

    whiteBoxCmdList->clearMutableAppendData();

    // clearMutableAppendData() frees every owned commandView and clears the container.
    // The NEO ULT leak listener validates that all allocated views were actually freed
    // (a missed if constexpr branch would leak and fail teardown).
    EXPECT_TRUE(whiteBoxCmdList->appendCmdsToPatch.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenOwningCommandViewVariantsWithNullCommandViewWhenClearMutableAppendDataThenNoDeallocationHappensAndContainerCleared) {
    auto whiteBoxCmdList = static_cast<MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> *>(mutableCommandList->base);

    // Seed appendCmdsToPatch with the owning variant types but leave commandView == nullptr.
    // clearMutableAppendData() must skip these (guarded by "if commandView != nullptr")
    // and must not call any deallocator on a null pointer.
    {
        PatchCbWaitEventLoadRegisterImm lri{};
        ASSERT_EQ(nullptr, lri.commandView);
        whiteBoxCmdList->appendCmdsToPatch.push_back(lri);

        PatchExternalCbWaitEventPreambleCounterLoadRegisterImm externalLri{};
        ASSERT_EQ(nullptr, externalLri.commandView);
        whiteBoxCmdList->appendCmdsToPatch.push_back(externalLri);

        PatchCbWaitEventSemaphoreWait cbEventSemWait{};
        ASSERT_EQ(nullptr, cbEventSemWait.commandView);
        whiteBoxCmdList->appendCmdsToPatch.push_back(cbEventSemWait);

        PatchExternalCbWaitEventPreambleCounterSemaphoreWait externalSemWait{};
        ASSERT_EQ(nullptr, externalSemWait.commandView);
        whiteBoxCmdList->appendCmdsToPatch.push_back(externalSemWait);

        PatchWaitEventSemaphoreWait semWait{};
        ASSERT_EQ(nullptr, semWait.commandView);
        whiteBoxCmdList->appendCmdsToPatch.push_back(semWait);

        PatchCbEventTimestampPostSyncSemaphoreWait timestampSemWait{};
        ASSERT_EQ(nullptr, timestampSemWait.commandView);
        whiteBoxCmdList->appendCmdsToPatch.push_back(timestampSemWait);
    }

    ASSERT_EQ(6u, whiteBoxCmdList->appendCmdsToPatch.size());

    // No commandView is allocated, so no deallocator must be invoked. The call must be safe
    // (no delete of nullptr) and must still clear the container.
    whiteBoxCmdList->clearMutableAppendData();

    EXPECT_TRUE(whiteBoxCmdList->appendCmdsToPatch.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenRegularCbEventRequiredFromApiAsExternalWhenAppendWaitOnEventsThenWaitEventVariableRegisteredAsExternal) {
    auto event = this->createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();

    auto externalCmdList = this->createMutableCmdList();
    // attach event to the external command list
    auto result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    L0::CmdListWaitEventParameters waitEventParams;
    waitEventParams.apiRequestForGraphExternal = true;

    result = this->mutableCommandList->appendWaitOnEvents(1, &eventHandle, waitEventParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &eventMutation = mutableCommandList->eventMutations[this->commandId - 1];
    ASSERT_EQ(1u, eventMutation.waitEvents.size());
    auto waitExternalVariable = eventMutation.waitEvents[0].eventVariable;

    EXPECT_TRUE(waitExternalVariable->getDesc().eventValue.apiRequiredExternal);
    EXPECT_TRUE(waitExternalVariable->getDesc().eventValue.isExternalFlag);
    ASSERT_EQ(2u, waitExternalVariable->getSemWaitList().size());

    // 1st sem wait is for patch preamble, 2nd is for event wait
    auto mutableSemWait = waitExternalVariable->getSemWaitList()[0];
    EXPECT_EQ(L0::MCL::MutableSemaphoreWait::cbEventWaitPatchPreambleCounter, mutableSemWait->getType());
    mutableSemWait = waitExternalVariable->getSemWaitList()[1];
    EXPECT_EQ(L0::MCL::MutableSemaphoreWait::cbEventWait, mutableSemWait->getType());

    if (this->lriRequired) {
        ASSERT_EQ(4u, waitExternalVariable->getLoadRegImmList().size());

        // 1st pair of lri is for patch preamble, 2nd pair is for event wait
        auto mutableLri = waitExternalVariable->getLoadRegImmList()[0];
        EXPECT_EQ(L0::MCL::MutableLoadRegisterImm::cbEventWaitLoadPatchPreambleCounter, mutableLri->getType());
        mutableLri = waitExternalVariable->getLoadRegImmList()[1];
        EXPECT_EQ(L0::MCL::MutableLoadRegisterImm::cbEventWaitLoadPatchPreambleCounter, mutableLri->getType());

        mutableLri = waitExternalVariable->getLoadRegImmList()[2];
        EXPECT_EQ(L0::MCL::MutableLoadRegisterImm::cbEventWaitLoadCounter, mutableLri->getType());
        mutableLri = waitExternalVariable->getLoadRegImmList()[3];
        EXPECT_EQ(L0::MCL::MutableLoadRegisterImm::cbEventWaitLoadCounter, mutableLri->getType());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenRegularCbEventSetWithPatchPreambleCounterWhenAppendWaitOnEventsThenPatchPreambleWaitMutableCommandsCaptured) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto event = this->createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();

    auto externalCmdList = this->createMutableCmdList();
    // attach event to the external command list
    auto result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint64_t cbDeviceGpuAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    uint64_t cbCounter = event->getInOrderExecBaseSignalValue();

    constexpr uint64_t preambleDeviceGpuAddress = 0xCD000;
    constexpr uint64_t preambleCounter = 4;

    MockGraphicsAllocation patchPreambleDeviceAllocation(nullptr, preambleDeviceGpuAddress, sizeof(uint64_t));
    event->getInOrderExecEventHelper().assignPatchPreambleData(preambleCounter, nullptr, 0, nullptr, preambleDeviceGpuAddress, &patchPreambleDeviceAllocation);

    // mutation point
    this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    L0::CmdListWaitEventParameters waitEventParams;
    waitEventParams.apiRequestForGraphExternal = false;

    result = this->mutableCommandList->appendWaitOnEvents(1, &eventHandle, waitEventParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(1u, mutableCommandList->eventMutations.size());
    auto &eventMutation = mutableCommandList->eventMutations[this->commandId - 1];
    ASSERT_EQ(1u, eventMutation.waitEvents.size());
    auto waitExternalVariable = eventMutation.waitEvents[0].eventVariable;

    EXPECT_FALSE(waitExternalVariable->getDesc().eventValue.apiRequiredExternal);
    EXPECT_FALSE(waitExternalVariable->getDesc().eventValue.isExternalFlag);
    ASSERT_EQ(2u, waitExternalVariable->getSemWaitList().size());

    // 1st sem wait is for patch preamble, 2nd is for event wait
    auto mutableSemWait = waitExternalVariable->getSemWaitList()[0];
    EXPECT_EQ(L0::MCL::MutableSemaphoreWait::cbEventWaitPatchPreambleCounter, mutableSemWait->getType());
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    EXPECT_EQ(preambleDeviceGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(mockMutableSemWait->semWait));

    mutableSemWait = waitExternalVariable->getSemWaitList()[1];
    EXPECT_EQ(L0::MCL::MutableSemaphoreWait::cbEventWait, mutableSemWait->getType());
    mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    EXPECT_EQ(cbDeviceGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(mockMutableSemWait->semWait));

    if (this->lriRequired) {
        ASSERT_EQ(4u, waitExternalVariable->getLoadRegImmList().size());

        // 1st pair of lri is for patch preamble, 2nd pair is for event wait
        auto mutableLri = waitExternalVariable->getLoadRegImmList()[0];
        EXPECT_EQ(L0::MCL::MutableLoadRegisterImm::cbEventWaitLoadPatchPreambleCounter, mutableLri->getType());
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(preambleCounter, lriCmd->getDataDword());
        mutableLri = waitExternalVariable->getLoadRegImmList()[1];
        EXPECT_EQ(L0::MCL::MutableLoadRegisterImm::cbEventWaitLoadPatchPreambleCounter, mutableLri->getType());

        mutableLri = waitExternalVariable->getLoadRegImmList()[2];
        EXPECT_EQ(L0::MCL::MutableLoadRegisterImm::cbEventWaitLoadCounter, mutableLri->getType());
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(cbCounter, lriCmd->getDataDword());
        mutableLri = waitExternalVariable->getLoadRegImmList()[3];
        EXPECT_EQ(L0::MCL::MutableLoadRegisterImm::cbEventWaitLoadCounter, mutableLri->getType());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderNoSem64Test,
            givenLriRequiredAndCbEventWithPatchPreambleCounterWhenRestoringPatchPreambleLoadRegisterImmsThenEachLoadsItsOwnRegister) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    if (!this->lriRequired) {
        GTEST_SKIP();
    }

    auto event = this->createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();

    auto externalCmdList = this->createMutableCmdList();
    EXPECT_EQ(ZE_RESULT_SUCCESS, externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams));
    EXPECT_EQ(ZE_RESULT_SUCCESS, externalCmdList->close());

    MockGraphicsAllocation patchPreambleDeviceAllocation(nullptr, 0xCD000, sizeof(uint64_t));
    event->getInOrderExecEventHelper().assignPatchPreambleData(4, nullptr, 0, nullptr, patchPreambleDeviceAllocation.getGpuAddress(), &patchPreambleDeviceAllocation);

    this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    EXPECT_EQ(ZE_RESULT_SUCCESS, this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId));

    L0::CmdListWaitEventParameters waitEventParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, this->mutableCommandList->appendWaitOnEvents(1, &eventHandle, waitEventParams));
    EXPECT_EQ(ZE_RESULT_SUCCESS, this->mutableCommandList->close());

    auto &eventMutation = mutableCommandList->eventMutations[this->commandId - 1];
    ASSERT_EQ(1u, eventMutation.waitEvents.size());
    auto &loadRegImmList = eventMutation.waitEvents[0].eventVariable->getLoadRegImmList();
    ASSERT_EQ(4u, loadRegImmList.size());

    const uint32_t expectedRegisters[] = {RegisterOffsets::csGprR0, RegisterOffsets::csGprR0 + 4};
    for (size_t i = 0; i < 2; i++) {
        auto mutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(loadRegImmList[i]);
        EXPECT_EQ(L0::MCL::MutableLoadRegisterImm::cbEventWaitLoadPatchPreambleCounter, mutableLri->getType());

        mutableLri->restore();

        auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mutableLri->loadRegImm);
        ASSERT_NE(nullptr, lriCmd);
        EXPECT_EQ(expectedRegisters[i], lriCmd->getRegisterOffset());
    }
}

using MutableCommandListDualStreamCopyOffloadTest = Test<MutableCommandListDualStreamCopyOffloadFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListDualStreamCopyOffloadTest,
            givenDualStreamCopyOffloadModeWhenCreatingInOrderMutableCmdListThenCopyOffloadIsEnabledAndMutableCopyOffloadSubCmdListCreated) {
    auto baseCmdList = mutableCommandList->getBase();
    EXPECT_TRUE(baseCmdList->isInOrderExecutionEnabled());
    EXPECT_TRUE(baseCmdList->isCopyOffloadEnabled());
    EXPECT_EQ(CopyOffloadModes::dualStream, baseCmdList->getCopyOffloadModeForOperation(true));

    constexpr size_t size = 8;
    void *usm1 = allocateUsm(size);
    void *usm2 = allocateUsm(size);

    CmdListMemoryCopyParams copyParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, this->mutableCommandList->appendMemoryCopy(usm1, usm2, size, nullptr, 0, nullptr, copyParams));
    EXPECT_TRUE(copyParams.copyOffloadAllowed);

    auto copyOffloadSubCmdList = this->mutableCommandList->getBase()->getCopyOffloadSubCmdList(DualStreamCopyOffload::copyOffloadReadIndex);
    ASSERT_NE(nullptr, copyOffloadSubCmdList);

    auto mutableCopyOffloadSubCmdList = copyOffloadSubCmdList->asMutable();
    ASSERT_NE(nullptr, mutableCopyOffloadSubCmdList);

    copyParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, this->mutableCommandList->appendMemoryCopy(usm1, usm2, size, nullptr, 0, nullptr, copyParams));
    EXPECT_TRUE(copyParams.copyOffloadAllowed);

    EXPECT_EQ(copyOffloadSubCmdList, this->mutableCommandList->getBase()->getCopyOffloadSubCmdList(DualStreamCopyOffload::copyOffloadReadIndex));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListDualStreamCopyOffloadTest,
            givenCopyOffloadHintOnDualStreamCopyOffloadModeWhenCreatingInOrderMutableCmdListThenCopyOffloadIsMutableAndInterceptsParentMutations) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    this->createCopyOffload = true;
    this->mutableCommandList = createMutableCmdList();
    auto whiteBoxBaseCmdList = static_cast<MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> *>(this->mutableCommandList->base);

    EXPECT_TRUE(whiteBoxBaseCmdList->isInOrderExecutionEnabled());
    EXPECT_TRUE(whiteBoxBaseCmdList->isCopyOffloadEnabled());

    constexpr size_t size = 8;
    void *usm1 = allocateUsm(size);
    void *usm2 = allocateUsm(size);

    this->eventsInPoolCount = 1;
    auto originalEvent = createTestEvent(false, false, false, false, false);
    auto originalEventAddress = originalEvent->getCompletionFieldGpuAddress(this->device);

    auto mutatedEvent = createTestEvent(false, false, false, false, false);
    auto mutatedEventAddress = mutatedEvent->getCompletionFieldGpuAddress(this->device);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListMemoryCopyParams copyParams = {};

    ASSERT_EQ(ZE_RESULT_SUCCESS, this->mutableCommandList->appendMemoryCopy(usm1, usm2, size, nullptr, 1, &this->eventHandles[0], copyParams));
    EXPECT_TRUE(copyParams.copyOffloadAllowed);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto copyOffloadAsMutable = whiteBoxBaseCmdList->copyOffloadSubCmdLists[DualStreamCopyOffload::copyOffloadReadIndex]->asMutable();
    ASSERT_NE(nullptr, copyOffloadAsMutable);
    auto copyOffloadMutableCmdList = static_cast<MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> *>(copyOffloadAsMutable);

    auto eventAllocation = originalEvent->getAllocation(this->device);

    auto &whiteboxComputeAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocationComputeIt = std::find_if(whiteboxComputeAllocations.addedAllocations.begin(),
                                                 whiteboxComputeAllocations.addedAllocations.end(),
                                                 [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                                     return ref.allocation == eventAllocation;
                                                 });
    EXPECT_EQ(whiteboxComputeAllocations.addedAllocations.end(), eventAllocationComputeIt);

    auto &basicComputeResidency = mutableCommandList->base->getCmdContainer().getResidencyContainer();
    auto basicComputeResidencyIt = std::find_if(basicComputeResidency.begin(),
                                                basicComputeResidency.end(),
                                                [&eventAllocation](const NEO::GraphicsAllocation *allocation) {
                                                    return allocation == eventAllocation;
                                                });
    EXPECT_EQ(basicComputeResidency.end(), basicComputeResidencyIt);

    auto &whiteBoxCopyOffloadAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(copyOffloadMutableCmdList->mutableAllocations);
    auto eventAllocationCopyOffloadIt = std::find_if(whiteBoxCopyOffloadAllocations.addedAllocations.begin(),
                                                     whiteBoxCopyOffloadAllocations.addedAllocations.end(),
                                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                                         return ref.allocation == eventAllocation;
                                                     });
    EXPECT_NE(whiteBoxCopyOffloadAllocations.addedAllocations.end(), eventAllocationCopyOffloadIt);

    auto &basicCopyOffloadResidency = copyOffloadMutableCmdList->getCmdContainer().getResidencyContainer();
    auto basicCopyOffloadResidencyIt = std::find_if(basicCopyOffloadResidency.begin(),
                                                    basicCopyOffloadResidency.end(),
                                                    [&eventAllocation](const NEO::GraphicsAllocation *allocation) {
                                                        return allocation == eventAllocation;
                                                    });
    EXPECT_NE(basicCopyOffloadResidency.end(), basicCopyOffloadResidencyIt);

    ASSERT_NE(0u, mutableCommandList->eventMutations.size());
    auto &selectedEventAppend = mutableCommandList->eventMutations[(commandId - 1)];
    ASSERT_NE(0u, selectedEventAppend.waitEvents.size());
    ASSERT_NE(nullptr, selectedEventAppend.waitEvents[0].eventVariable);
    EXPECT_EQ(originalEvent, selectedEventAppend.waitEvents[0].event);
    auto waitEventVar = selectedEventAppend.waitEvents[0].eventVariable;
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    EXPECT_EQ(originalEventAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto mutatedEventAllocation = mutatedEvent->getAllocation(this->device);

    eventAllocationComputeIt = std::find_if(whiteboxComputeAllocations.addedAllocations.begin(),
                                            whiteboxComputeAllocations.addedAllocations.end(),
                                            [&mutatedEventAllocation](const L0::MCL::AllocationReference &ref) {
                                                return ref.allocation == mutatedEventAllocation;
                                            });
    EXPECT_EQ(whiteboxComputeAllocations.addedAllocations.end(), eventAllocationComputeIt);

    basicComputeResidencyIt = std::find_if(basicComputeResidency.begin(),
                                           basicComputeResidency.end(),
                                           [&mutatedEventAllocation](const NEO::GraphicsAllocation *allocation) {
                                               return allocation == mutatedEventAllocation;
                                           });
    EXPECT_EQ(basicComputeResidency.end(), basicComputeResidencyIt);

    eventAllocationCopyOffloadIt = std::find_if(whiteBoxCopyOffloadAllocations.addedAllocations.begin(),
                                                whiteBoxCopyOffloadAllocations.addedAllocations.end(),
                                                [&mutatedEventAllocation](const L0::MCL::AllocationReference &ref) {
                                                    return ref.allocation == mutatedEventAllocation;
                                                });
    EXPECT_NE(whiteBoxCopyOffloadAllocations.addedAllocations.end(), eventAllocationCopyOffloadIt);

    basicCopyOffloadResidencyIt = std::find_if(basicCopyOffloadResidency.begin(),
                                               basicCopyOffloadResidency.end(),
                                               [&mutatedEventAllocation](const NEO::GraphicsAllocation *allocation) {
                                                   return allocation == mutatedEventAllocation;
                                               });
    EXPECT_NE(basicCopyOffloadResidency.end(), basicCopyOffloadResidencyIt);

    EXPECT_EQ(mutatedEventAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListDualStreamCopyOffloadTest,
            givenCopyOffloadHintOnDualStreamCopyOffloadModeWithPatchPreambleWhenCreatingInOrderMutableCmdListAndAppendMemoryCopyThenCommandsMutatedIntoCommandViewsUsingCorrectEngine) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    this->createCopyOffload = true;
    this->mutableCommandList = createMutableCmdList();
    auto whiteBoxBaseCmdList = static_cast<MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> *>(this->mutableCommandList->base);
    whiteBoxBaseCmdList->setupPatchPreambleEnabled(true);

    EXPECT_TRUE(whiteBoxBaseCmdList->isInOrderExecutionEnabled());
    EXPECT_TRUE(whiteBoxBaseCmdList->isCopyOffloadEnabled());

    constexpr size_t size = 8;
    void *usm1 = allocateUsm(size);
    void *usm2 = allocateUsm(size);

    auto originalEvent = createTestEvent(true, false, false, false, false);
    auto mutatedEvent = createTestEvent(true, false, false, false, false);

    auto originalCommandList = this->createMutableCmdList();
    CmdListWaitEventParameters waitEventsParameters = {};
    CmdListSignalEventParameters signalEventParameters = {};
    auto result = originalCommandList->appendBarrier(this->eventHandles[0], 0, nullptr, waitEventsParameters, signalEventParameters);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto originalEventAddress = originalEvent->getInOrderExecEventHelper().getBaseDeviceAddress() + originalEvent->getInOrderAllocationOffset();
    auto originalEventAllocation = originalEvent->getInOrderExecEventHelper().getDeviceCounterAllocation();

    auto mutatedCommandList = this->createMutableCmdList();
    result = mutatedCommandList->appendBarrier(this->eventHandles[1], 0, nullptr, waitEventsParameters, signalEventParameters);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto mutatedEventAddress = mutatedEvent->getInOrderExecEventHelper().getBaseDeviceAddress() + mutatedEvent->getInOrderAllocationOffset();
    auto mutatedEventAllocation = mutatedEvent->getInOrderExecEventHelper().getDeviceCounterAllocation();

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListMemoryCopyParams copyParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, this->mutableCommandList->appendMemoryCopy(usm1, usm2, size, nullptr, 1, &this->eventHandles[0], copyParams));
    EXPECT_TRUE(copyParams.copyOffloadAllowed);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto copyOffloadAsMutable = whiteBoxBaseCmdList->copyOffloadSubCmdLists[DualStreamCopyOffload::copyOffloadReadIndex]->asMutable();
    ASSERT_NE(nullptr, copyOffloadAsMutable);
    auto copyOffloadMutableCmdList = static_cast<MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> *>(copyOffloadAsMutable);

    auto &whiteBoxCopyOffloadAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(copyOffloadMutableCmdList->mutableAllocations);

    auto eventAllocationIt = std::find_if(whiteBoxCopyOffloadAllocations.addedAllocations.begin(),
                                          whiteBoxCopyOffloadAllocations.addedAllocations.end(),
                                          [&originalEventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == originalEventAllocation;
                                          });
    EXPECT_NE(whiteBoxCopyOffloadAllocations.addedAllocations.end(), eventAllocationIt);

    auto &basicCopyOffloadResidency = copyOffloadMutableCmdList->getCmdContainer().getResidencyContainer();
    auto basicCopyOffloadResidencyIt = std::find_if(basicCopyOffloadResidency.begin(),
                                                    basicCopyOffloadResidency.end(),
                                                    [&originalEventAllocation](const NEO::GraphicsAllocation *allocation) {
                                                        return allocation == originalEventAllocation;
                                                    });
    EXPECT_NE(basicCopyOffloadResidency.end(), basicCopyOffloadResidencyIt);

    ASSERT_NE(0u, mutableCommandList->eventMutations.size());
    auto &selectedEventAppend = mutableCommandList->eventMutations[(commandId - 1)];
    ASSERT_NE(0u, selectedEventAppend.waitEvents.size());
    ASSERT_NE(nullptr, selectedEventAppend.waitEvents[0].eventVariable);
    EXPECT_EQ(originalEvent, selectedEventAppend.waitEvents[0].event);
    auto waitEventVar = selectedEventAppend.waitEvents[0].eventVariable;
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    ASSERT_NE(nullptr, mockMutableSemWait->commandView);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->commandView);
    EXPECT_EQ(originalEventAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    const size_t expectedLriSize = this->lriRequired ? 2 : 0;
    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());
    for (auto &mutableLri : waitEventVar->getLoadRegImmList()) {
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        ASSERT_NE(nullptr, mockMutableLri->commandView);
        EXPECT_TRUE(mockMutableLri->isCopy);
    }

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocationIt = std::find_if(whiteBoxCopyOffloadAllocations.addedAllocations.begin(),
                                     whiteBoxCopyOffloadAllocations.addedAllocations.end(),
                                     [&mutatedEventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == mutatedEventAllocation;
                                     });
    EXPECT_NE(whiteBoxCopyOffloadAllocations.addedAllocations.end(), eventAllocationIt);

    basicCopyOffloadResidencyIt = std::find_if(basicCopyOffloadResidency.begin(),
                                               basicCopyOffloadResidency.end(),
                                               [&mutatedEventAllocation](const NEO::GraphicsAllocation *allocation) {
                                                   return allocation == mutatedEventAllocation;
                                               });
    EXPECT_NE(basicCopyOffloadResidency.end(), basicCopyOffloadResidencyIt);

    ASSERT_NE(nullptr, mockMutableSemWait->commandView);
    EXPECT_EQ(mutatedEventAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListDualStreamCopyOffloadTest,
            givenCopyOffloadSubCmdListWhenAddingAsyncMutationElementThenElementIsRedirectedOnlyWhenRequested) {
    this->createCopyOffload = true;
    this->mutableCommandList = createMutableCmdList();

    constexpr size_t size = 8;
    void *usm1 = allocateUsm(size);
    void *usm2 = allocateUsm(size);

    CmdListMemoryCopyParams copyParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, this->mutableCommandList->appendMemoryCopy(usm1, usm2, size, nullptr, 0, nullptr, copyParams));
    EXPECT_TRUE(copyParams.copyOffloadAllowed);

    auto baseCmdList = this->mutableCommandList->getBase();
    auto copyOffloadSubCmdList = baseCmdList->getCopyOffloadSubCmdList(DualStreamCopyOffload::copyOffloadReadIndex);
    ASSERT_NE(nullptr, copyOffloadSubCmdList);

    auto &computeAsyncPatchContainer = baseCmdList->getAsyncPatchContainer();
    auto &copyAsyncPatchContainer = copyOffloadSubCmdList->getAsyncPatchContainer();
    computeAsyncPatchContainer.clear();
    copyAsyncPatchContainer.clear();

    uint8_t computeHostSource[16] = {};
    uint8_t copyHostSource[16] = {};
    constexpr uint64_t computeGpuDestination = 0x10000;
    constexpr uint64_t copyGpuDestination = 0x20000;

    this->mutableCommandList->addAsyncMutationElement(computeGpuDestination, computeHostSource, sizeof(computeHostSource), nullptr);
    ASSERT_EQ(1u, computeAsyncPatchContainer.size());
    EXPECT_EQ(0u, copyAsyncPatchContainer.size());
    EXPECT_EQ(computeGpuDestination, computeAsyncPatchContainer[0].gpuDestinationAddress);
    EXPECT_EQ(computeHostSource, computeAsyncPatchContainer[0].hostSourceAddress);

    this->mutableCommandList->addAsyncMutationElement(copyGpuDestination, copyHostSource, sizeof(copyHostSource), copyOffloadSubCmdList);
    EXPECT_EQ(1u, computeAsyncPatchContainer.size());
    ASSERT_EQ(1u, copyAsyncPatchContainer.size());
    EXPECT_EQ(copyGpuDestination, copyAsyncPatchContainer[0].gpuDestinationAddress);
    EXPECT_EQ(copyHostSource, copyAsyncPatchContainer[0].hostSourceAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListDualStreamCopyOffloadTest,
            givenCopyOffloadHintWithPatchPreambleWhenMutatingCbWaitEventOfCopyOffloadAppendThenAsyncPatchesLandInCopyOffloadSubCmdList) {
    this->createCopyOffload = true;
    this->mutableCommandList = createMutableCmdList();
    auto whiteBoxBaseCmdList = static_cast<MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> *>(this->mutableCommandList->base);
    whiteBoxBaseCmdList->setupPatchPreambleEnabled(true);

    constexpr size_t size = 8;
    void *usm1 = allocateUsm(size);
    void *usm2 = allocateUsm(size);

    createTestEvent(true, false, false, false, false);
    createTestEvent(true, false, false, false, false);

    CmdListWaitEventParameters waitEventsParameters = {};
    CmdListSignalEventParameters signalEventParameters = {};
    auto originalCommandList = this->createMutableCmdList();
    auto result = originalCommandList->appendBarrier(this->eventHandles[0], 0, nullptr, waitEventsParameters, signalEventParameters);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto mutatedCommandList = this->createMutableCmdList();
    result = mutatedCommandList->appendBarrier(this->eventHandles[1], 0, nullptr, waitEventsParameters, signalEventParameters);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListMemoryCopyParams copyParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, this->mutableCommandList->appendMemoryCopy(usm1, usm2, size, nullptr, 1, &this->eventHandles[0], copyParams));
    EXPECT_TRUE(copyParams.copyOffloadAllowed);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto copyOffloadSubCmdList = whiteBoxBaseCmdList->getCopyOffloadSubCmdList(DualStreamCopyOffload::copyOffloadReadIndex);
    ASSERT_NE(nullptr, copyOffloadSubCmdList);

    auto &computeAsyncPatchContainer = whiteBoxBaseCmdList->getAsyncPatchContainer();
    auto &copyAsyncPatchContainer = copyOffloadSubCmdList->getAsyncPatchContainer();
    computeAsyncPatchContainer.clear();
    copyAsyncPatchContainer.clear();

    auto &selectedEventAppend = mutableCommandList->eventMutations[(commandId - 1)];
    ASSERT_NE(0u, selectedEventAppend.waitEvents.size());
    auto waitEventVar = selectedEventAppend.waitEvents[0].eventVariable;
    ASSERT_NE(nullptr, waitEventVar);
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    std::vector<void *> expectedHostSources;
    expectedHostSources.push_back(static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(waitEventVar->getSemWaitList()[0])->commandView);
    for (auto &mutableLri : waitEventVar->getLoadRegImmList()) {
        expectedHostSources.push_back(static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri)->commandView);
    }

    result = mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, computeAsyncPatchContainer.size());
    ASSERT_EQ(expectedHostSources.size(), copyAsyncPatchContainer.size());
    for (auto &patch : copyAsyncPatchContainer) {
        EXPECT_NE(expectedHostSources.end(), std::find(expectedHostSources.begin(), expectedHostSources.end(), patch.hostSourceAddress));
    }
}

struct MutableCommandListDualStreamCopyOffloadWriteStreamFixture : public MutableCommandListDualStreamCopyOffloadFixture {
    void setUp() {
        NEO::debugManager.flags.CopyOffloadWriteEngine.set(1);
        MutableCommandListDualStreamCopyOffloadFixture::setUp();

        this->createCopyOffload = true;
        this->mutableCommandList = createMutableCmdList();
        this->mutableCommandList->getBase()->enableCopyOffloadWriteStream();
    }

    void *allocateHostUsm(size_t size) {
        ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
        void *hostUsm = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, size, 4096u, &hostUsm));
        deviceUsmAllocations.push_back(hostUsm);
        return hostUsm;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    MutableCommandListCoreFamily<gfxCoreFamily> *getMutableCopyOffloadSubCmdList(size_t queueIndex) {
        auto copyOffloadSubCmdList = this->mutableCommandList->getBase()->getCopyOffloadSubCmdList(queueIndex);
        if (copyOffloadSubCmdList == nullptr) {
            return nullptr;
        }
        return static_cast<MutableCommandListCoreFamily<gfxCoreFamily> *>(copyOffloadSubCmdList->asMutable());
    }

    static bool isResident(L0::CommandList &cmdList, const NEO::GraphicsAllocation *allocation) {
        auto &residencyContainer = cmdList.getCmdContainer().getResidencyContainer();
        return std::find(residencyContainer.begin(), residencyContainer.end(), allocation) != residencyContainer.end();
    }
};

using MutableCommandListDualStreamCopyOffloadWriteStreamTest = Test<MutableCommandListDualStreamCopyOffloadWriteStreamFixture>;

HWTEST2_F(MutableCommandListDualStreamCopyOffloadWriteStreamTest,
          givenWriteStreamEnabledOnMutableCmdListWhenAppendingCopiesThenCopyToHostIsRecordedIntoMutableWriteStreamAndCopyToDeviceIntoMutableReadStream, IsCRI) {
    constexpr size_t size = 8;
    void *hostUsm = allocateHostUsm(size);
    void *deviceUsm = allocateDeviceUsm(size);
    ASSERT_NE(nullptr, deviceUsm);

    auto baseCmdList = this->mutableCommandList->getBase();

    CmdListMemoryCopyParams copyParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, this->mutableCommandList->appendMemoryCopy(hostUsm, deviceUsm, size, nullptr, 0, nullptr, copyParams));
    EXPECT_TRUE(copyParams.copyOffloadAllowed);

    auto writeCmdList = getMutableCopyOffloadSubCmdList<FamilyType::gfxCoreFamily>(DualStreamCopyOffload::copyOffloadWriteIndex);
    ASSERT_NE(nullptr, writeCmdList);
    EXPECT_EQ(baseCmdList->getCopyOffloadSubCmdList(DualStreamCopyOffload::copyOffloadWriteIndex), copyParams.copyOffloadSubCmdList);
    EXPECT_EQ(nullptr, baseCmdList->getCopyOffloadSubCmdList(DualStreamCopyOffload::copyOffloadReadIndex));

    copyParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, this->mutableCommandList->appendMemoryCopy(deviceUsm, hostUsm, size, nullptr, 0, nullptr, copyParams));
    EXPECT_TRUE(copyParams.copyOffloadAllowed);

    auto readCmdList = getMutableCopyOffloadSubCmdList<FamilyType::gfxCoreFamily>(DualStreamCopyOffload::copyOffloadReadIndex);
    ASSERT_NE(nullptr, readCmdList);
    EXPECT_NE(writeCmdList, readCmdList);
    EXPECT_EQ(baseCmdList->getCopyOffloadSubCmdList(DualStreamCopyOffload::copyOffloadReadIndex), copyParams.copyOffloadSubCmdList);
}

HWTEST2_F(MutableCommandListDualStreamCopyOffloadWriteStreamTest,
          givenWriteStreamEnabledOnMutableCmdListWhenMutatingWaitEventOfCopyToHostThenWaitEventResidencyIsKeptInWriteStream, IsCRI) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    constexpr size_t size = 8;
    void *hostUsm = allocateHostUsm(size);
    void *deviceUsm = allocateDeviceUsm(size);
    ASSERT_NE(nullptr, deviceUsm);

    this->eventsInPoolCount = 1;
    auto originalEvent = createTestEvent(false, false, false, false, false);
    auto mutatedEvent = createTestEvent(false, false, false, false, false);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_WAIT_EVENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));

    CmdListMemoryCopyParams copyParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, this->mutableCommandList->appendMemoryCopy(hostUsm, deviceUsm, size, nullptr, 1, &this->eventHandles[0], copyParams));
    EXPECT_TRUE(copyParams.copyOffloadAllowed);
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto baseCmdList = this->mutableCommandList->getBase();
    auto writeCmdList = getMutableCopyOffloadSubCmdList<FamilyType::gfxCoreFamily>(DualStreamCopyOffload::copyOffloadWriteIndex);
    ASSERT_NE(nullptr, writeCmdList);
    EXPECT_EQ(nullptr, baseCmdList->getCopyOffloadSubCmdList(DualStreamCopyOffload::copyOffloadReadIndex));

    auto originalEventAllocation = originalEvent->getAllocation(this->device);
    EXPECT_TRUE(isResident(*writeCmdList, originalEventAllocation));
    EXPECT_FALSE(isResident(*baseCmdList, originalEventAllocation));

    auto &selectedEventAppend = mutableCommandList->eventMutations[(commandId - 1)];
    ASSERT_NE(0u, selectedEventAppend.waitEvents.size());
    auto waitEventVar = selectedEventAppend.waitEvents[0].eventVariable;
    ASSERT_NE(nullptr, waitEventVar);
    EXPECT_EQ(writeCmdList, waitEventVar->getDesc().copyOffloadSubCmdList);
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(waitEventVar->getSemWaitList()[0])->semWait);

    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &this->eventHandles[1]));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto mutatedEventAllocation = mutatedEvent->getAllocation(this->device);
    EXPECT_TRUE(isResident(*writeCmdList, mutatedEventAllocation));
    EXPECT_FALSE(isResident(*baseCmdList, mutatedEventAllocation));
    EXPECT_EQ(nullptr, baseCmdList->getCopyOffloadSubCmdList(DualStreamCopyOffload::copyOffloadReadIndex));
    EXPECT_EQ(mutatedEvent->getCompletionFieldGpuAddress(this->device), NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWTEST2_F(MutableCommandListDualStreamCopyOffloadWriteStreamTest,
          givenWriteStreamEnabledOnMutableCmdListWithPatchPreambleWhenMutatingCbWaitEventOfCopyToHostThenAsyncPatchesLandInWriteStream, IsCRI) {
    this->mutableCommandList->getBase()->setupPatchPreambleEnabled(true);

    constexpr size_t size = 8;
    void *hostUsm = allocateHostUsm(size);
    void *deviceUsm = allocateDeviceUsm(size);
    ASSERT_NE(nullptr, deviceUsm);

    createTestEvent(true, false, false, false, false);
    createTestEvent(true, false, false, false, false);

    CmdListWaitEventParameters waitEventsParameters = {};
    CmdListSignalEventParameters signalEventParameters = {};
    auto originalCommandList = this->createMutableCmdList();
    ASSERT_EQ(ZE_RESULT_SUCCESS, originalCommandList->appendBarrier(this->eventHandles[0], 0, nullptr, waitEventsParameters, signalEventParameters));
    auto mutatedCommandList = this->createMutableCmdList();
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutatedCommandList->appendBarrier(this->eventHandles[1], 0, nullptr, waitEventsParameters, signalEventParameters));

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));

    CmdListMemoryCopyParams copyParams = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, this->mutableCommandList->appendMemoryCopy(hostUsm, deviceUsm, size, nullptr, 1, &this->eventHandles[0], copyParams));
    EXPECT_TRUE(copyParams.copyOffloadAllowed);
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto baseCmdList = this->mutableCommandList->getBase();
    auto writeCmdList = getMutableCopyOffloadSubCmdList<FamilyType::gfxCoreFamily>(DualStreamCopyOffload::copyOffloadWriteIndex);
    ASSERT_NE(nullptr, writeCmdList);
    EXPECT_TRUE(writeCmdList->isPatchPreambleEnabled());

    auto &computeAsyncPatchContainer = baseCmdList->getAsyncPatchContainer();
    auto &writeAsyncPatchContainer = writeCmdList->getAsyncPatchContainer();
    computeAsyncPatchContainer.clear();
    writeAsyncPatchContainer.clear();

    auto &selectedEventAppend = mutableCommandList->eventMutations[(commandId - 1)];
    ASSERT_NE(0u, selectedEventAppend.waitEvents.size());
    auto waitEventVar = selectedEventAppend.waitEvents[0].eventVariable;
    ASSERT_NE(nullptr, waitEventVar);
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    std::vector<void *> expectedHostSources;
    expectedHostSources.push_back(static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(waitEventVar->getSemWaitList()[0])->commandView);
    for (auto &mutableLri : waitEventVar->getLoadRegImmList()) {
        expectedHostSources.push_back(static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri)->commandView);
    }

    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandWaitEvents(commandId, 1, &this->eventHandles[1]));

    EXPECT_EQ(0u, computeAsyncPatchContainer.size());
    ASSERT_EQ(expectedHostSources.size(), writeAsyncPatchContainer.size());
    for (auto &patch : writeAsyncPatchContainer) {
        EXPECT_NE(expectedHostSources.end(), std::find(expectedHostSources.begin(), expectedHostSources.end(), patch.hostSourceAddress));
    }
}

// ISA in local memory has no CPU pointer; with content it is also locked, as after a read back
struct IsaInLocalMemory {
    IsaInLocalMemory(NEO::GraphicsAllocation &allocation, size_t contentSize = 0u)
        : allocation(allocation), cpuPtr(allocation.getUnderlyingBuffer()), content(contentSize) {
        allocation.setCpuPtrAndGpuAddress(nullptr, allocation.getGpuAddress());
        for (size_t i = 0; i < content.size(); i++) {
            content[i] = static_cast<uint8_t>(i * 7 + 3);
        }
        if (false == content.empty()) {
            allocation.lock(content.data());
        }
    }
    ~IsaInLocalMemory() {
        if (allocation.isLocked()) {
            allocation.unlock();
        }
        allocation.setCpuPtrAndGpuAddress(cpuPtr, allocation.getGpuAddress());
    }

    NEO::GraphicsAllocation &allocation;
    void *cpuPtr;
    std::vector<uint8_t> content;
};

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelIsaAllocationWithCpuPointerWhenGettingKernelDataThenIsaIsReferencedInAllocation) {
    auto isaAllocation = mockKernelImmData->getIsaGraphicsAllocation();

    auto kernelData = mutableCommandList->getKernelData(kernel.get());
    EXPECT_EQ(isaAllocation->getUnderlyingBuffer(), kernelData->kernelIsa.begin());
    EXPECT_EQ(mockKernelImmData->getIsaSize(), kernelData->kernelIsa.size());
    EXPECT_EQ(nullptr, kernelData->isaAllocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelIsaAllocationWithoutCpuPointerWhenGettingKernelDataThenIsaAllocationIsKeptForReadBack) {
    IsaInLocalMemory isa{*mockKernelImmData->getIsaGraphicsAllocation()};

    auto kernelData = mutableCommandList->getKernelData(kernel.get());
    EXPECT_TRUE(kernelData->kernelIsa.empty());
    EXPECT_EQ(&isa.allocation, kernelData->isaAllocation);
    EXPECT_EQ(mockKernelImmData->getIsaSize(), kernelData->isaSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelIsaAllocationWithoutCpuPointerWhenReadingKernelIsaFromAllocationsThenIsaIsCopiedToHost) {
    IsaInLocalMemory isa{*mockKernelImmData->getIsaGraphicsAllocation(), mockKernelImmData->getIsaSize()};
    const auto &isaContent = isa.content;

    auto kernelData = mutableCommandList->getKernelData(kernel.get());
    auto kernelDataWithCpuIsa = mutableCommandList->getKernelData(kernel2.get());
    auto cpuIsaBegin = kernelDataWithCpuIsa->kernelIsa.begin();

    EXPECT_TRUE(mutableCommandList->readKernelIsaFromAllocations());
    ASSERT_EQ(isaContent.size(), kernelData->kernelIsa.size());
    EXPECT_EQ(kernelData->kernelIsaHostCopy.data(), kernelData->kernelIsa.begin());
    EXPECT_EQ(0, memcmp(isaContent.data(), kernelData->kernelIsa.begin(), isaContent.size()));
    EXPECT_EQ(cpuIsaBegin, kernelDataWithCpuIsa->kernelIsa.begin());
    EXPECT_TRUE(kernelDataWithCpuIsa->kernelIsaHostCopy.empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelIsaReadBackFailsWhenGettingNativeBinaryThenErrorIsReturned) {
    IsaInLocalMemory isa{*mockKernelImmData->getIsaGraphicsAllocation()};

    mutableCommandList->getKernelData(kernel.get());

    size_t binarySize = 0u;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, mutableCommandList->getNativeBinary(nullptr, &binarySize, nullptr, 0u));
    EXPECT_EQ(0u, binarySize);
}

struct IsaWithoutCpuPtrMemoryManager : public ModuleImmutableDataFixture::MockImmutableMemoryManager {
    using MockImmutableMemoryManager::MockImmutableMemoryManager;
    using NEO::MockMemoryManager::allocateGraphicsMemoryWithProperties;

    NEO::GraphicsAllocation *allocateGraphicsMemoryWithProperties(const NEO::AllocationProperties &properties) override {
        auto allocation = MockImmutableMemoryManager::allocateGraphicsMemoryWithProperties(properties);
        if (allocation != nullptr && allocateKernelIsaWithoutCpuPtr && properties.allocationType == NEO::AllocationType::kernelIsa) {
            allocationsWithoutCpuPtr.push_back({allocation, allocation->getUnderlyingBuffer()});
            allocation->setCpuPtrAndGpuAddress(nullptr, allocation->getGpuAddress());
        }
        return allocation;
    }

    bool copyMemoryToAllocation(NEO::GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) override {
        if (allocateKernelIsaWithoutCpuPtr && graphicsAllocation->getUnderlyingBuffer() == nullptr) {
            auto src = reinterpret_cast<const uint8_t *>(memoryToCopy);
            copiesToAllocationsWithoutCpuPtr.push_back({graphicsAllocation, destinationOffset, std::vector<uint8_t>(src, src + sizeToCopy)});
        }
        return MockImmutableMemoryManager::copyMemoryToAllocation(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy);
    }

    void restoreCpuPtrs() {
        for (auto &[allocation, cpuPtr] : allocationsWithoutCpuPtr) {
            allocation->setCpuPtrAndGpuAddress(cpuPtr, allocation->getGpuAddress());
        }
        allocationsWithoutCpuPtr.clear();
    }

    struct CopyToAllocationWithoutCpuPtr {
        NEO::GraphicsAllocation *allocation;
        size_t offset;
        std::vector<uint8_t> data;
    };
    std::vector<CopyToAllocationWithoutCpuPtr> copiesToAllocationsWithoutCpuPtr;
    std::vector<std::pair<NEO::GraphicsAllocation *, void *>> allocationsWithoutCpuPtr;
    bool allocateKernelIsaWithoutCpuPtr = false;
};

struct MutableCommandListIsaWithoutCpuPtrFixture : public MutableCommandListFixture<false, -1> {
    MockImmutableMemoryManager *createMemoryManager(NEO::ExecutionEnvironment &executionEnvironment) override {
        isaMemoryManager = new IsaWithoutCpuPtrMemoryManager(executionEnvironment);
        return isaMemoryManager;
    }

    IsaWithoutCpuPtrMemoryManager *isaMemoryManager = nullptr;
};

using MutableCommandListIsaWithoutCpuPtrTest = Test<MutableCommandListIsaWithoutCpuPtrFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListIsaWithoutCpuPtrTest,
            givenKernelIsaWithoutCpuPointerWhenNativeBinaryIsSavedAndLoadedIntoInstructionHeapWithoutCpuPointerThenIsaIsSavedAndUploaded) {
    const auto isaSize = mockKernelImmData->getIsaSize();
    auto isa = std::make_unique<IsaInLocalMemory>(*mockKernelImmData->getIsaGraphicsAllocation(), isaSize);
    const auto isaContent = isa->content;

    ze_mutable_command_id_exp_desc_t mutableCommandIdExpDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    mutableCommandIdExpDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;
    auto result = L0::zeCommandListGetNextCommandIdExp(mutableCommandList->toHandle(), &mutableCommandIdExpDesc, &commandId);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    size_t binarySize = 0u;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNativeBinary(nullptr, &binarySize, nullptr, 0u));
    ASSERT_NE(0u, binarySize);
    std::vector<uint8_t> binary(binarySize);
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNativeBinary(binary.data(), &binarySize, nullptr, 0u));

    isa.reset();

    L0::MCL::Program::Decoder::MclDecoder savedProgram;
    ASSERT_TRUE(savedProgram.decodeElf({binary.data(), binary.size()}));
    savedProgram.parseSymbols();
    ASSERT_EQ(1u, savedProgram.getKernelData().size());
    auto &savedKernel = savedProgram.getKernelData()[0];
    ASSERT_EQ(isaSize, savedKernel.kernelIsa.size());
    EXPECT_EQ(0, memcmp(isaContent.data(), savedKernel.kernelIsa.begin(), isaSize));

    VariableBackup<NEO::BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationBackup(
        &NEO::BlitHelperFunctions::blitMemoryToAllocation,
        [](const NEO::Device &device, NEO::GraphicsAllocation *memory, size_t offset, const void *hostPtr, const Vec3<size_t> &size) -> NEO::BlitOperationResult {
            return NEO::BlitOperationResult::unsupported;
        });

    auto loadedCommandList = createMutableCmdList();
    isaMemoryManager->copiesToAllocationsWithoutCpuPtr.clear();
    isaMemoryManager->allocateKernelIsaWithoutCpuPtr = true;
    result = loadedCommandList->loadFromBinary(binary.data(), binary.size());
    isaMemoryManager->allocateKernelIsaWithoutCpuPtr = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(1u, isaMemoryManager->allocationsWithoutCpuPtr.size());
    auto instructionHeapAllocation = isaMemoryManager->allocationsWithoutCpuPtr[0].first;
    EXPECT_EQ(instructionHeapAllocation, loadedCommandList->allocs.ihAlloc.get());

    ASSERT_EQ(1u, isaMemoryManager->copiesToAllocationsWithoutCpuPtr.size());
    auto &upload = isaMemoryManager->copiesToAllocationsWithoutCpuPtr[0];
    EXPECT_EQ(instructionHeapAllocation, upload.allocation);
    EXPECT_EQ(0u, upload.offset);

    ASSERT_EQ(1u, loadedCommandList->kernelData.size());
    auto &loadedKernel = *loadedCommandList->kernelData[0];
    ASSERT_LE(loadedKernel.kernelStartOffset + isaSize, upload.data.size());
    EXPECT_EQ(0, memcmp(isaContent.data(), upload.data.data() + loadedKernel.kernelStartOffset, isaSize));

    isaMemoryManager->restoreCpuPtrs();
}

class MclBinaryLoadTest : public MutableCommandListTest {
  protected:
    static void addOutOfRangeVarSymbol(MockMclEncoder &encoder) {
        using namespace L0::MCL::Program;
        Symbols::VariableSymbolValue varSymbol{};
        varSymbol.id = std::numeric_limits<uint32_t>::max();
        encoder.addSymbol(Symbols::SymbolNames::varPrefix.str() + "var", Sections::SectionType::shtUndef, Symbols::SymbolType::var, varSymbol.data, 4u);
    }

    static void addOutOfRangeDispatchSymbol(MockMclEncoder &encoder) {
        using namespace L0::MCL::Program;
        Symbols::DispatchSymbolValue dispatchSymbol{};
        dispatchSymbol.dispatchId = std::numeric_limits<uint16_t>::max();
        encoder.addSymbol(Symbols::SymbolNames::dispatchPrefix.str() + "0", Sections::SectionType::shtUndef, Symbols::SymbolType::dispatch, dispatchSymbol.data, 0u);
    }

    static void expectNativeBinaryIsRejected(MutableCommandList &mutableCmdList, const std::vector<uint8_t> &binary) {
        auto commandStream = mutableCmdList.base->getCmdContainer().getCommandStream();
        const auto usedBefore = commandStream->getUsed();
        const auto cpuBaseBefore = commandStream->getCpuBase();
        const auto variablesBefore = mutableCmdList.variableStorage.size();
        const auto ihAllocBefore = mutableCmdList.allocs.ihAlloc.get();

        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NATIVE_BINARY, mutableCmdList.loadFromBinary(binary.data(), binary.size()));

        EXPECT_EQ(usedBefore, commandStream->getUsed());
        EXPECT_EQ(cpuBaseBefore, commandStream->getCpuBase());
        EXPECT_EQ(variablesBefore, mutableCmdList.variableStorage.size());
        EXPECT_EQ(ihAllocBefore, mutableCmdList.allocs.ihAlloc.get());
    }
};

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MclBinaryLoadTest,
            givenNativeBinaryWithVarSymbolIdOutOfRangeWhenLoadingFromBinaryThenInvalidNativeBinaryIsReturnedAndNothingIsLoaded) {
    MockMclEncoder encoder;
    addOutOfRangeVarSymbol(encoder);

    expectNativeBinaryIsRejected(*mutableCommandList, encoder.encodeProgram());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MclBinaryLoadTest,
            givenNativeBinaryWithDispatchIdOutOfRangeWhenLoadingFromBinaryThenInvalidNativeBinaryIsReturnedAndNothingIsLoaded) {
    MockMclEncoder encoder;
    addOutOfRangeDispatchSymbol(encoder);

    expectNativeBinaryIsRejected(*mutableCommandList, encoder.encodeProgram());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MclBinaryLoadTest,
            givenRejectedNativeBinaryWhenLoadingValidNativeBinaryThenCommandListIsLoaded) {
    IsaInLocalMemory isa{*mockKernelImmData->getIsaGraphicsAllocation(), mockKernelImmData->getIsaSize()};
    ze_mutable_command_id_exp_desc_t mutableCommandIdExpDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    mutableCommandIdExpDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListGetNextCommandIdExp(mutableCommandList->toHandle(), &mutableCommandIdExpDesc, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    size_t binarySize = 0u;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNativeBinary(nullptr, &binarySize, nullptr, 0u));
    std::vector<uint8_t> validBinary(binarySize);
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNativeBinary(validBinary.data(), &binarySize, nullptr, 0u));

    auto commandStream = mutableCommandList->base->getCmdContainer().getCommandStream();
    const auto used = commandStream->getUsed();
    const auto commandBuffer = static_cast<const uint8_t *>(commandStream->getCpuBase());
    ASSERT_GT(used, 0u);
    ASSERT_NE(nullptr, commandBuffer);
    const std::vector<uint8_t> commandsBefore(commandBuffer, commandBuffer + used);

    MockMclEncoder encoder;
    addOutOfRangeVarSymbol(encoder);
    encoder.program.cs = commandsBefore;
    encoder.program.cs.front() ^= 1u;
    const auto invalidBinary = encoder.encodeProgram();

    const auto copyCallsBefore = memoryManager->copyMemoryToAllocationCalledTimes;
    expectNativeBinaryIsRejected(*mutableCommandList, invalidBinary);
    EXPECT_EQ(copyCallsBefore, memoryManager->copyMemoryToAllocationCalledTimes);
    ASSERT_EQ(used, commandStream->getUsed());
    ASSERT_NE(nullptr, commandStream->getCpuBase());
    EXPECT_EQ(0, std::memcmp(commandsBefore.data(), commandStream->getCpuBase(), used));

    auto loadedCommandList = createMutableCmdList();
    expectNativeBinaryIsRejected(*loadedCommandList, invalidBinary);
    EXPECT_EQ(ZE_RESULT_SUCCESS, loadedCommandList->loadFromBinary(validBinary.data(), validBinary.size()));
    EXPECT_EQ(mutableCommandList->kernelData.size(), loadedCommandList->kernelData.size());
    EXPECT_EQ(mutableCommandList->dispatches.size(), loadedCommandList->dispatches.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MclBinaryLoadTest,
            givenBinaryWhichIsNotElfWhenLoadingFromBinaryThenInvalidNativeBinaryIsReturned) {
    const std::array<uint8_t, 64> notElf = {};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NATIVE_BINARY, mutableCommandList->loadFromBinary(notElf.data(), notElf.size()));
}

} // namespace ult
} // namespace L0
