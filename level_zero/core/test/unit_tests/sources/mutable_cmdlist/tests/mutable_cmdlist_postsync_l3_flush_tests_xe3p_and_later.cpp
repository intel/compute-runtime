/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_cmdlist_fixture.h"

namespace L0 {
namespace ult {

using MutableCommandListPostSyncL3FlushTest = Test<MutableCommandListFixture<true, true>>;

template <typename GfxFamily>
void expectPostSyncFlushes(typename GfxFamily::DefaultWalkerType *walker,
                           NEO::PostSyncFlushMask flushMask,
                           bool l2TransientFlush,
                           bool l2Flush) {
    for (uint32_t postSyncId = 0; postSyncId < NEO::EncodePostSync<GfxFamily>::maxPostSyncOperations; postSyncId++) {
        auto verifyPostSync = [&](auto &postSync) {
            const bool isFlush = NEO::EncodePostSync<GfxFamily>::isPostSyncFlush(flushMask, postSyncId);
            EXPECT_EQ(isFlush && l2TransientFlush, postSync.getL2TransientFlush()) << "PostSync" << postSyncId;
            EXPECT_EQ(isFlush && l2Flush, postSync.getL2Flush()) << "PostSync" << postSyncId;
        };

        switch (postSyncId) {
        case 0:
            verifyPostSync(walker->getPostSync());
            break;
        case 1:
            verifyPostSync(walker->getPostSyncOpn1());
            break;
        case 2:
            verifyPostSync(walker->getPostSyncOpn2());
            break;
        case 3:
            verifyPostSync(walker->getPostSyncOpn3());
            break;
        }
    }
}

HWTEST2_F(MutableCommandListPostSyncL3FlushTest,
          givenHostVisibleEventAndSystemSharedBufferArgWhenArgIsMutatedThenL2TransientFlushIsUpdated,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b0010u;

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DisableSystemPointerKernelArgument.set(0);
    NEO::debugManager.flags.EnableRecoverablePageFaults.set(1);
    NEO::debugManager.flags.EnableSharedSystemUsmSupport.set(1);

    auto &hwInfo = *neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = UnifiedSharedMemoryFlags::access;
    mutableCommandList = createMutableCmdList();
    ASSERT_TRUE(mutableCommandList->getBase()->areSharedSystemAllocationsAllowed());
    sharedSystemMemCapabilities = 0u;
    ASSERT_FALSE(neoDevice->areSharedSystemAllocationsAllowed());
    ASSERT_TRUE(mutableCommandList->getBase()->areSharedSystemAllocationsAllowed());

    auto *event = createTestEvent(true, true, false, false, false);
    ASSERT_NE(nullptr, event);

    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);

    void *systemShared = reinterpret_cast<void *>(0x12340000);
    void *deviceUsm = allocateDeviceUsm(4096);
    ASSERT_NE(nullptr, deviceUsm);

    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(0, sizeof(void *), &systemShared));
    ASSERT_EQ(nullptr, kernel->getArgumentsResidencyContainer()[0]);
    ASSERT_EQ(systemShared, kernel->getKernelArgInfos()[0].value);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), testGroupCount, event->toHandle(), 0, nullptr, testLaunchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto *walker = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());

    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, this->l3FlushAfterPostSyncEnabled, false);

    ze_mutable_kernel_argument_exp_desc_t argDesc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    argDesc.commandId = commandId;
    argDesc.argIndex = 0;
    argDesc.argSize = sizeof(void *);
    argDesc.pArgValue = &deviceUsm;
    mutableCommandsDesc.pNext = &argDesc;

    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());
    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    argDesc.pArgValue = &systemShared;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());
    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, this->l3FlushAfterPostSyncEnabled, false);

    argDesc.pArgValue = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());
    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    argDesc.pArgValue = &systemShared;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());
    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, this->l3FlushAfterPostSyncEnabled, false);
}

HWTEST2_F(MutableCommandListPostSyncL3FlushTest,
          givenHostVisibleEventWhenLastHostBufferArgMutatedToDeviceThenL2TransientFlushIsCleared,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b0010u;

    bool signalHostEvent = true;

    auto *event = createTestEvent(true, signalHostEvent, false, false, false);
    ASSERT_NE(nullptr, event);

    resizeKernelArg(2);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    prepareKernelArg(1, L0::MCL::VariableType::buffer, kernelAllMask);

    // arg0 = host USM
    // arg1 = device USM
    void *hostUsm = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, 4096, 4096, &hostUsm));
    void *deviceUsm0 = allocateDeviceUsm(4096);
    void *deviceUsm1 = allocateDeviceUsm(4096);
    ASSERT_NE(nullptr, deviceUsm0);
    ASSERT_NE(nullptr, deviceUsm1);

    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(0, sizeof(void *), &hostUsm));
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(1, sizeof(void *), &deviceUsm1));

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), testGroupCount, event->toHandle(), 0, nullptr, testLaunchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto *walker = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());

    // initial state
    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, signalHostEvent && this->l3FlushAfterPostSyncEnabled, false);

    // mutate arg0 host -> device
    ze_mutable_kernel_argument_exp_desc_t argDesc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    argDesc.commandId = commandId;
    argDesc.argIndex = 0;
    argDesc.argSize = sizeof(void *);
    argDesc.pArgValue = &deviceUsm0;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    context->freeMem(hostUsm);
}

HWTEST2_F(MutableCommandListPostSyncL3FlushTest,
          givenHostVisibleEventWhenDeviceBufferArgMutatedToHostThenL2TransientFlushIsSet,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b0010u;

    bool signalHostEvent = true;
    auto *event = createTestEvent(true, signalHostEvent, false, false, false);
    ASSERT_NE(nullptr, event);

    resizeKernelArg(2);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    prepareKernelArg(1, L0::MCL::VariableType::buffer, kernelAllMask);

    // both args device USM
    void *deviceUsm0 = allocateDeviceUsm(4096);
    void *deviceUsm1 = allocateDeviceUsm(4096);
    ASSERT_NE(nullptr, deviceUsm0);
    ASSERT_NE(nullptr, deviceUsm1);

    void *hostUsm = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, 4096, 4096, &hostUsm));

    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(0, sizeof(void *), &deviceUsm0));
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(1, sizeof(void *), &deviceUsm1));

    kernel->privateState.kernelRequiresGenerationOfLocalIdsByRuntime = false;
    kernel->privateState.reservePerThreadDataForWholeThreadGroup(0);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), testGroupCount, event->toHandle(), 0, nullptr, testLaunchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto *walker = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());

    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    // mutate arg0 device -> host
    ze_mutable_kernel_argument_exp_desc_t argDesc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    argDesc.commandId = commandId;
    argDesc.argIndex = 0;
    argDesc.argSize = sizeof(void *);
    argDesc.pArgValue = &hostUsm;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    // one host alloc after mutation, flush required on the duplicated host counter
    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, signalHostEvent && this->l3FlushAfterPostSyncEnabled, false);

    context->freeMem(hostUsm);
}

HWTEST2_F(MutableCommandListPostSyncL3FlushTest,
          givenTwoHostBufferArgsWhenMutatedToDeviceOneByOneThenL2TransientFlushIsClearedAfterLastHostArg,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b0010u;

    bool signalHostEvent = true;
    auto *event = createTestEvent(true, signalHostEvent, false, false, false);
    ASSERT_NE(nullptr, event);

    resizeKernelArg(2);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    prepareKernelArg(1, L0::MCL::VariableType::buffer, kernelAllMask);

    // both args host USM
    void *hostUsm0 = nullptr;
    void *hostUsm1 = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, 4096, 4096, &hostUsm0));
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, 4096, 4096, &hostUsm1));
    void *deviceUsm0 = allocateDeviceUsm(4096);
    void *deviceUsm1 = allocateDeviceUsm(4096);
    ASSERT_NE(nullptr, deviceUsm0);
    ASSERT_NE(nullptr, deviceUsm1);

    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(0, sizeof(void *), &hostUsm0));
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(1, sizeof(void *), &hostUsm1));

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), testGroupCount, event->toHandle(), 0, nullptr, testLaunchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto *walker = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());

    // two host allocs
    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, signalHostEvent && this->l3FlushAfterPostSyncEnabled, false);

    // mutate arg0 host -> device
    ze_mutable_kernel_argument_exp_desc_t argDesc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    argDesc.commandId = commandId;
    argDesc.argIndex = 0;
    argDesc.argSize = sizeof(void *);
    argDesc.pArgValue = &deviceUsm0;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    // one host alloc left
    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, signalHostEvent && this->l3FlushAfterPostSyncEnabled, false);

    // mutate arg1 host -> device
    argDesc.argIndex = 1;
    argDesc.pArgValue = &deviceUsm1;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    context->freeMem(hostUsm0);
    context->freeMem(hostUsm1);
}

HWTEST2_F(MutableCommandListPostSyncL3FlushTest,
          givenHostVisibleEventWhenImportedBufferArgMutatedThenL2FlushIsSetCorrectly,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b0010u;

    bool signalHostEvent = true;
    auto *event = createTestEvent(true, signalHostEvent, false, false, false);
    ASSERT_NE(nullptr, event);

    resizeKernelArg(2);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    prepareKernelArg(1, L0::MCL::VariableType::buffer, kernelAllMask);

    void *importedUsm = allocateDeviceUsm(2 * MemoryConstants::pageSize);
    void *deviceUsm0 = allocateDeviceUsm(4096);
    void *deviceUsm1 = allocateDeviceUsm(4096);
    ASSERT_NE(nullptr, importedUsm);
    ASSERT_NE(nullptr, deviceUsm0);
    ASSERT_NE(nullptr, deviceUsm1);

    auto *importedAlloc = getUsmAllocation(importedUsm);
    auto *deviceAllocation0 = getUsmAllocation(deviceUsm0);
    auto *deviceAllocation1 = getUsmAllocation(deviceUsm1);
    ASSERT_NE(nullptr, importedAlloc);
    ASSERT_NE(nullptr, deviceAllocation0);
    ASSERT_NE(nullptr, deviceAllocation1);
    ASSERT_NE(importedAlloc, deviceAllocation0);
    ASSERT_NE(importedAlloc, deviceAllocation1);
    importedAlloc->setIsImported();

    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(0, sizeof(void *), &importedUsm));
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(1, sizeof(void *), &deviceUsm1));

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), testGroupCount, event->toHandle(), 0, nullptr, testLaunchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto *walker = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());

    // one imported alloc
    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, false, signalHostEvent && this->l3FlushAfterPostSyncEnabled);

    // mutate arg0 imported -> device usm
    ze_mutable_kernel_argument_exp_desc_t argDesc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    argDesc.commandId = commandId;
    argDesc.argIndex = 0;
    argDesc.argSize = sizeof(void *);
    argDesc.pArgValue = &deviceUsm0;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    // no imported allocs
    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    // mutate arg0 plain device -> imported
    argDesc.pArgValue = &importedUsm;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, false, signalHostEvent && this->l3FlushAfterPostSyncEnabled);
}

HWTEST2_F(MutableCommandListPostSyncL3FlushTest,
          givenHostVisibleEventWhenBufferArgMutatedDeviceToDeviceThenL2TransientFlushIsNotSet,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    bool signalHostEvent = true;
    auto *event = createTestEvent(true, signalHostEvent, false, false, false);
    ASSERT_NE(nullptr, event);

    resizeKernelArg(2);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    prepareKernelArg(1, L0::MCL::VariableType::buffer, kernelAllMask);

    // device USM args
    void *deviceUsm0 = allocateDeviceUsm(4096);
    void *deviceUsm0New = allocateDeviceUsm(4096);
    void *deviceUsm1 = allocateDeviceUsm(4096);
    ASSERT_NE(nullptr, deviceUsm0);
    ASSERT_NE(nullptr, deviceUsm0New);
    ASSERT_NE(nullptr, deviceUsm1);

    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(0, sizeof(void *), &deviceUsm0));
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(1, sizeof(void *), &deviceUsm1));

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), testGroupCount, event->toHandle(), 0, nullptr, testLaunchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto *walker = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());

    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    // mutate arg0 device USM -> device USM
    ze_mutable_kernel_argument_exp_desc_t argDesc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    argDesc.commandId = commandId;
    argDesc.argIndex = 0;
    argDesc.argSize = sizeof(void *);
    argDesc.pArgValue = &deviceUsm0New;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);
}

HWTEST2_F(MutableCommandListPostSyncL3FlushTest,
          givenHostVisibleEventWhenImportedHostBufferArgIsMutatedToDeviceThenBothL2FlushesAreCleared,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b0010u;

    auto *event = createTestEvent(true, true, false, false, false);
    ASSERT_NE(nullptr, event);

    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);

    void *importedHostUsm = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, 4096, 4096, &importedHostUsm));
    auto *importedHostAllocation = getUsmAllocation(importedHostUsm);
    ASSERT_NE(nullptr, importedHostAllocation);
    importedHostAllocation->setIsImported();

    void *deviceUsm = allocateDeviceUsm(4096);
    ASSERT_NE(nullptr, deviceUsm);

    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(0, sizeof(void *), &importedHostUsm));

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), testGroupCount, event->toHandle(), 0, nullptr, testLaunchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto *walker = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, this->l3FlushAfterPostSyncEnabled, this->l3FlushAfterPostSyncEnabled);

    ze_mutable_kernel_argument_exp_desc_t argDesc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    argDesc.commandId = commandId;
    argDesc.argIndex = 0;
    argDesc.argSize = sizeof(void *);
    argDesc.pArgValue = &deviceUsm;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    context->freeMem(importedHostUsm);
}

HWTEST2_F(MutableCommandListPostSyncL3FlushTest,
          givenHostSignalScopeEventAndMutableDeviceBufferWhenMutatedToHostThenL2TransientFlushIsUpdated,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b0010u;

    auto *event = createTestEvent(true, true, false, false, false);
    ASSERT_NE(nullptr, event);

    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);

    void *deviceUsm = allocateDeviceUsm(4096);
    ASSERT_NE(nullptr, deviceUsm);

    void *hostUsm = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, 4096, 4096, &hostUsm));

    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(0, sizeof(void *), &deviceUsm));

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), testGroupCount, event->toHandle(), 0, nullptr, testLaunchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto *walker = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    ze_mutable_kernel_argument_exp_desc_t argDesc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    argDesc.commandId = commandId;
    argDesc.argIndex = 0;
    argDesc.argSize = sizeof(void *);
    argDesc.pArgValue = &hostUsm;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, this->l3FlushAfterPostSyncEnabled, false);

    argDesc.pArgValue = &deviceUsm;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    context->freeMem(hostUsm);
}

HWTEST2_F(MutableCommandListPostSyncL3FlushTest,
          givenHostSignalScopeEventAndDeviceBufferWhenMutatedToImportedDeviceAllocationThenL2FlushIsUpdated,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b0010u;

    auto *event = createTestEvent(true, true, false, false, false);
    ASSERT_NE(nullptr, event);

    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);

    void *deviceUsm = allocateDeviceUsm(4096);
    void *importedUsm = allocateDeviceUsm(2 * MemoryConstants::pageSize);
    ASSERT_NE(nullptr, deviceUsm);
    ASSERT_NE(nullptr, importedUsm);

    auto *deviceAllocation = getUsmAllocation(deviceUsm);
    auto *importedAllocation = getUsmAllocation(importedUsm);
    ASSERT_NE(nullptr, deviceAllocation);
    ASSERT_NE(nullptr, importedAllocation);
    ASSERT_NE(deviceAllocation, importedAllocation);
    importedAllocation->setIsImported();

    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(0, sizeof(void *), &deviceUsm));

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), testGroupCount, event->toHandle(), 0, nullptr, testLaunchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto *walker = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    ze_mutable_kernel_argument_exp_desc_t argDesc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    argDesc.commandId = commandId;
    argDesc.argIndex = 0;
    argDesc.argSize = sizeof(void *);
    argDesc.pArgValue = &importedUsm;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, false, this->l3FlushAfterPostSyncEnabled);

    argDesc.pArgValue = &deviceUsm;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);
}

HWTEST2_F(MutableCommandListPostSyncL3FlushTest,
          givenHostBufferAndImportedArgWhenNoHostSignalEventThenL2FlushesAreNotProgrammed,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    bool signalHostEvent = false;
    auto *event = createTestEvent(true, signalHostEvent, false, false, false);
    ASSERT_NE(nullptr, event);

    resizeKernelArg(2);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    prepareKernelArg(1, L0::MCL::VariableType::buffer, kernelAllMask);

    // arg0 = host USM
    // arg1 = imported device USM
    void *hostUsm0 = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, 4096, 4096, &hostUsm0));
    void *importedUsm = allocateDeviceUsm(2 * MemoryConstants::pageSize);
    void *deviceUsm0 = allocateDeviceUsm(4096);
    void *deviceUsm1 = allocateDeviceUsm(4096);
    ASSERT_NE(nullptr, importedUsm);
    ASSERT_NE(nullptr, deviceUsm0);
    ASSERT_NE(nullptr, deviceUsm1);

    auto *importedAlloc = getUsmAllocation(importedUsm);
    ASSERT_NE(nullptr, importedAlloc);
    importedAlloc->setIsImported();

    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(0, sizeof(void *), &hostUsm0));
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(1, sizeof(void *), &importedUsm));

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), testGroupCount, event->toHandle(), 0, nullptr, testLaunchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto *walker = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());

    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    // mutate arg0 host USM-> device USM
    ze_mutable_kernel_argument_exp_desc_t argDesc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    argDesc.commandId = commandId;
    argDesc.argIndex = 0;
    argDesc.argSize = sizeof(void *);
    argDesc.pArgValue = &deviceUsm0;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    // mutate arg1 imported -> device USM
    argDesc.argIndex = 1;
    argDesc.pArgValue = &deviceUsm1;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);

    context->freeMem(hostUsm0);
}

HWTEST2_F(MutableCommandListPostSyncL3FlushTest,
          givenRegularHostScopeEventWhenBufferArgIsMutatedThenEveryHostVisiblePostSyncIsUpdated,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using POSTSYNC_DATA_2 = typename FamilyType::POSTSYNC_DATA_2;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b0110u;

    auto *event = createTestEvent(false, true, false, false, false);
    ASSERT_NE(nullptr, event);
    auto &inOrderExecInfo = mutableCommandList->base->getInOrderExecInfo();
    ASSERT_TRUE(inOrderExecInfo->isHostStorageDuplicated());

    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);

    void *hostUsm = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, 4096, 4096, &hostUsm));
    void *deviceUsm = allocateDeviceUsm(4096);
    ASSERT_NE(nullptr, deviceUsm);
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(0, sizeof(void *), &hostUsm));

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), testGroupCount, event->toHandle(), 0, nullptr, testLaunchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto *walker = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    EXPECT_EQ(inOrderExecInfo->getBaseDeviceAddress(), walker->getPostSync().getDestinationAddress());
    EXPECT_EQ(inOrderExecInfo->getBaseHostGpuAddress(), walker->getPostSyncOpn1().getDestinationAddress());
    EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_WRITE_IMMEDIATE_DATA, walker->getPostSyncOpn2().getOperation());
    EXPECT_EQ(event->getPacketAddress(device), walker->getPostSyncOpn2().getDestinationAddress());
    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, this->l3FlushAfterPostSyncEnabled, false);

    ze_mutable_kernel_argument_exp_desc_t argDesc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    argDesc.commandId = commandId;
    argDesc.argIndex = 0;
    argDesc.argSize = sizeof(void *);
    argDesc.pArgValue = &deviceUsm;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);
    context->freeMem(hostUsm);
}

HWTEST2_F(MutableCommandListPostSyncL3FlushTest,
          givenAggregatedExternalHostScopeEventWhenBufferArgIsMutatedThenHostCounterAndExternalCounterPostSyncsAreUpdated,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using POSTSYNC_DATA_2 = typename FamilyType::POSTSYNC_DATA_2;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b0110u;

    auto *event = createTestEvent(true, true, false, true, false);
    ASSERT_NE(nullptr, event);
    ASSERT_GT(event->getInOrderIncrementValue(1), 0u);
    auto &inOrderExecInfo = mutableCommandList->base->getInOrderExecInfo();
    ASSERT_TRUE(inOrderExecInfo->isHostStorageDuplicated());

    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);

    void *hostUsm = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, 4096, 4096, &hostUsm));
    void *deviceUsm = allocateDeviceUsm(4096);
    ASSERT_NE(nullptr, deviceUsm);
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setArgBuffer(0, sizeof(void *), &hostUsm));

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), testGroupCount, event->toHandle(), 0, nullptr, testLaunchParams));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto *walker = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    EXPECT_EQ(inOrderExecInfo->getBaseDeviceAddress(), walker->getPostSync().getDestinationAddress());
    EXPECT_EQ(inOrderExecInfo->getBaseHostGpuAddress(), walker->getPostSyncOpn1().getDestinationAddress());
    EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_ATOMIC_OPN, walker->getPostSyncOpn2().getOperation());
    EXPECT_EQ(event->getInOrderExecEventHelper().getBaseDeviceAddress(), walker->getPostSyncOpn2().getDestinationAddress());
    expectPostSyncFlushes<FamilyType>(walker, postSyncFlushMask, this->l3FlushAfterPostSyncEnabled, false);

    ze_mutable_kernel_argument_exp_desc_t argDesc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    argDesc.commandId = commandId;
    argDesc.argIndex = 0;
    argDesc.argSize = sizeof(void *);
    argDesc.pArgValue = &deviceUsm;
    mutableCommandsDesc.pNext = &argDesc;
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc));
    ASSERT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    expectPostSyncFlushes<FamilyType>(walker, 0, false, false);
    context->freeMem(hostUsm);
}

} // namespace ult
} // namespace L0
