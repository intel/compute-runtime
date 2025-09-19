/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include "test_mode.h"

namespace L0 {
namespace ult {

using AUBHelloWorldL0 = Test<AUBFixtureL0>;
TEST_F(AUBHelloWorldL0, whenAppendMemoryCopyIsCalledThenMemoryIsProperlyCopied) {
    uint8_t size = 8;
    uint8_t val = 255;

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                           1,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto srcMemory = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    auto dstMemory = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);

    memset(srcMemory, val, size);
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryCopy(dstMemory, srcMemory, size, nullptr, 0, nullptr, copyParams);
    commandList->close();
    auto pHCmdList = std::make_unique<ze_command_list_handle_t>(commandList->toHandle());

    pCmdq->executeCommandLists(1, pHCmdList.get(), nullptr, false, nullptr, nullptr);
    pCmdq->synchronize(std::numeric_limits<uint32_t>::max());

    EXPECT_TRUE(csr->expectMemory(dstMemory, srcMemory, size, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));

    driverHandle->svmAllocsManager->freeSVMAlloc(srcMemory);
    driverHandle->svmAllocsManager->freeSVMAlloc(dstMemory);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            AUBHelloWorldL0,
            GivenPatchPreambleQueueAndTwoCommandListsWhenAppendMemoryCopyFromSourceToStageAndFromStageToDestinationThenDataIsCopied) {
    constexpr size_t size = 64;
    constexpr uint8_t val1 = 255;
    constexpr uint8_t val2 = 127;

    ze_result_t returnValue;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    auto contextHandle = context->toHandle();
    auto deviceHandle = device->toHandle();

    void *srcMemory1 = nullptr;
    void *dstMemory1 = nullptr;
    void *srcMemory2 = nullptr;
    void *dstMemory2 = nullptr;
    void *stageMemory = nullptr;

    returnValue = zeMemAllocHost(contextHandle, &hostDesc, size, 1, &srcMemory1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    memset(srcMemory1, val1, size);
    returnValue = zeMemAllocHost(contextHandle, &hostDesc, size, 1, &srcMemory2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    memset(srcMemory2, val2, size);
    returnValue = zeMemAllocHost(contextHandle, &hostDesc, size, 1, &dstMemory1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemAllocHost(contextHandle, &hostDesc, size, 1, &dstMemory2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    memset(dstMemory1, 0, size);
    memset(dstMemory2, 0, size);
    returnValue = zeMemAllocDevice(contextHandle, &deviceDesc, size, 1, deviceHandle, &stageMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_event_pool_handle_t eventPoolHandle = nullptr;
    ze_event_handle_t eventHandle = nullptr;

    ze_event_pool_desc_t eventPoolDesc{ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 1;

    returnValue = zeEventPoolCreate(contextHandle, &eventPoolDesc, 0, nullptr, &eventPoolHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_event_desc_t eventDesc{ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = 0;

    returnValue = zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_command_list_desc_t commandListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};

    ze_command_list_handle_t commandListHandle = nullptr;
    // create command list and copy from srcMemory1 to stageMemory - signal event
    returnValue = zeCommandListCreate(contextHandle, deviceHandle, &commandListDesc, &commandListHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListAppendMemoryCopy(commandListHandle, stageMemory, srcMemory1, size, eventHandle, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListClose(commandListHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    // create second command list and copy from stageMemory to dstMemory1 - wait for event
    ze_command_list_handle_t commandListHandle2 = nullptr;
    returnValue = zeCommandListCreate(contextHandle, deviceHandle, &commandListDesc, &commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListAppendMemoryCopy(commandListHandle2, dstMemory1, stageMemory, size, nullptr, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListClose(commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_command_list_handle_t commandLists[] = {commandListHandle, commandListHandle2};

    uint32_t commandListCount = sizeof(commandLists) / sizeof(commandLists[0]);

    pCmdq->setPatchingPreamble(true, false);

    auto queueHandle = pCmdq->toHandle();

    // execute command lists and program expected memory srcMemory1 to dstMemory1
    returnValue = zeCommandQueueExecuteCommandLists(queueHandle, commandListCount, commandLists, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandQueueSynchronize(queueHandle, std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_TRUE(csr->expectMemory(dstMemory1, srcMemory1, size, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));

    // reset command lists and event
    returnValue = zeCommandListReset(commandListHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListReset(commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeEventHostReset(eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    // copy from srcMemory2 to stageMemory - signal event
    returnValue = zeCommandListAppendMemoryCopy(commandListHandle, stageMemory, srcMemory2, size, eventHandle, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListClose(commandListHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    // copy from stageMemory to dstMemory2 - wait for event
    returnValue = zeCommandListAppendMemoryCopy(commandListHandle2, dstMemory2, stageMemory, size, nullptr, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListClose(commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    // execute command lists and program expected memory srcMemory2 to dstMemory2
    returnValue = zeCommandQueueExecuteCommandLists(queueHandle, commandListCount, commandLists, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandQueueSynchronize(queueHandle, std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_TRUE(csr->expectMemory(dstMemory2, srcMemory2, size, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));

    // destroy resources
    returnValue = zeEventDestroy(eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeEventPoolDestroy(eventPoolHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemFree(contextHandle, srcMemory1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemFree(contextHandle, srcMemory2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemFree(contextHandle, dstMemory1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemFree(contextHandle, dstMemory2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemFree(contextHandle, stageMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListDestroy(commandListHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeCommandListDestroy(commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            AUBHelloWorldL0,
            GivenCmdListExperimentalBufferAutomodifyWhenDispatchingBlockMemoryToSetThenTotalBlockIsModifiedUsingSingleStoreDataCommand) {
    constexpr size_t size = 16 * sizeof(uint64_t);
    constexpr uint8_t val = 255;

    ze_result_t returnValue;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    auto contextHandle = context->toHandle();

    void *srcMemory = nullptr;
    void *dstMemory = nullptr;
    void *testMemory = nullptr;
    void *refMemory = nullptr;

    returnValue = zeMemAllocHost(contextHandle, &hostDesc, size, 1, &srcMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    memset(srcMemory, val, size);
    returnValue = zeMemAllocHost(contextHandle, &hostDesc, size, 1, &dstMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    size_t testMemorySize = 256;
    returnValue = zeMemAllocHost(contextHandle, &hostDesc, testMemorySize, 1, &testMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    memset(testMemory, 0x7F, testMemorySize);
    returnValue = zeMemAllocHost(contextHandle, &hostDesc, testMemorySize, 1, &refMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    uint32_t fillDstValue = *reinterpret_cast<uint32_t *>(srcMemory);

    auto cmdListStream = commandList->getCmdContainer().getCommandStream();
    auto &residencyContainer = commandList->getCmdContainer().getResidencyContainer();

    auto dstAllocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(dstMemory);
    auto dstAllocation = dstAllocData->gpuAllocations.getDefaultGraphicsAllocation();
    residencyContainer.push_back(dstAllocation);

    auto srcAllocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(srcMemory);
    auto testAllocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(testMemory);
    auto refAllocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(refMemory);

    auto srcAllocation = srcAllocData->gpuAllocations.getDefaultGraphicsAllocation();
    auto testAllocation = testAllocData->gpuAllocations.getDefaultGraphicsAllocation();
    auto refAllocation = refAllocData->gpuAllocations.getDefaultGraphicsAllocation();

    residencyContainer.push_back(srcAllocation);
    residencyContainer.push_back(testAllocation);
    residencyContainer.push_back(refAllocation);

    constexpr size_t sdiCount = size / sizeof(uint64_t);
    constexpr size_t sdiCmdAddressDwordOffset = 1;
    uint32_t *refDwordMemory = reinterpret_cast<uint32_t *>(refMemory);

    const uint32_t oneMmio = RegisterOffsets::csGprR5; // MMIO that keeps value 1 to decrement loop counter
    auto mathAluOneMmio = AluRegisters::gpr5;          // ALU register that keeps value 1 to decrement loop counter

    NEO::LriHelper<FamilyType>::program(cmdListStream, oneMmio, 1, NEO::EncodeSetMMIO<FamilyType>::isRemapApplicable(oneMmio), false);
    NEO::LriHelper<FamilyType>::program(cmdListStream, oneMmio + 4, 0, NEO::EncodeSetMMIO<FamilyType>::isRemapApplicable(oneMmio + 4), false);

    const uint32_t sdiCounterMmio = RegisterOffsets::csGprR4; // MMIO that keeps loop counter value
    auto mathAluSdiCounterMmio = AluRegisters::gpr4;          // ALU register that keeps loop counter value
    // initialize loop counter register
    NEO::LriHelper<FamilyType>::program(cmdListStream,
                                        sdiCounterMmio,
                                        sdiCount,
                                        NEO::EncodeSetMMIO<FamilyType>::isRemapApplicable(sdiCounterMmio),
                                        false);

    uint64_t dstMemoryGpuVa = reinterpret_cast<uint64_t>(dstMemory);
    uint32_t dstMemoryLowerDword = static_cast<uint32_t>(dstMemoryGpuVa & 0xFFFFFFFF);
    refDwordMemory[0] = dstMemoryLowerDword + (sdiCount - 1) * sizeof(uint64_t); // expected dst address after loop
    refDwordMemory[2] = 1;                                                       // expected counter value after loop

    const uint32_t sdiDwordDstAddressMmio = RegisterOffsets::csGprR1; // MMIO that keeps qword increased dst address patched in SDI command
    auto mathAluDstAddressMmio = AluRegisters::gpr1;                  // ALU register that keeps qword increased dst address patched in SDI command
    NEO::LriHelper<FamilyType>::program(cmdListStream,
                                        sdiDwordDstAddressMmio,
                                        dstMemoryLowerDword,
                                        NEO::EncodeSetMMIO<FamilyType>::isRemapApplicable(sdiDwordDstAddressMmio),
                                        false);

    // load register with dst address offset
    const uint32_t offsetMmio = RegisterOffsets::csGprR2; // MMIO register that keeps dst address offset
    auto mathAluOffsetMmio = AluRegisters::gpr2;          // ALU register that keeps dst address offset
    uint32_t offset = 8;                                  // next SDI dword address offset
    NEO::LriHelper<FamilyType>::program(cmdListStream,
                                        offsetMmio,
                                        offset,
                                        NEO::EncodeSetMMIO<FamilyType>::isRemapApplicable(offsetMmio),
                                        false);

    // disable prefetching in arb check
    NEO::EncodeMiArbCheck<FamilyType>::program(*cmdListStream, true);

    // start of the loop
    uint64_t startLoopBufferAddress = cmdListStream->getCurrentGpuAddressPosition();

    uint64_t mainSdiAddress = cmdListStream->getCurrentGpuAddressPosition();
    // address to patch SDI command with new dst address
    uint64_t modifySdiDstAddress = mainSdiAddress + (sdiCmdAddressDwordOffset * sizeof(uint32_t));

    NEO::EncodeStoreMemory<FamilyType>::programStoreDataImm(*cmdListStream,
                                                            reinterpret_cast<uint64_t>(dstMemory),
                                                            fillDstValue,
                                                            fillDstValue,
                                                            true,
                                                            false,
                                                            nullptr);

    // verify sdiCount MMIO counter reaches end of the loop
    NEO::EncodeAluHelper<FamilyType, 4> aluHelper;
    aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srca, mathAluSdiCounterMmio);
    aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srcb, mathAluOneMmio);
    aluHelper.setNextAlu(AluRegisters::opcodeSub);
    aluHelper.setNextAlu(AluRegisters::opcodeStore, AluRegisters::gpr6, AluRegisters::zf);
    aluHelper.copyToCmdStream(*cmdListStream);

    // store result of the comparison in csPredicateResult2 MMIO
    NEO::EncodeSetMMIO<FamilyType>::encodeREG(*cmdListStream, RegisterOffsets::csPredicateResult2, RegisterOffsets::csGprR6, false);
    // predicate the jump to the end of the loop
    NEO::EncodeMiPredicate<FamilyType>::encode(*cmdListStream, NEO::MiPredicateType::noopOnResult2Clear);

    // conditional batch buffer start to end loop jump
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    MI_BATCH_BUFFER_START *bbStartEndLoop = cmdListStream->getSpaceForCmd<MI_BATCH_BUFFER_START>();

    // if not exiting the loop, disable predicate
    NEO::EncodeMiPredicate<FamilyType>::encode(*cmdListStream, NEO::MiPredicateType::disable);

    // decrement loop counter
    NEO::EncodeMathMMIO<FamilyType>::encodeDecrement(*cmdListStream, mathAluSdiCounterMmio, false);

    // add qword to dstMemory MMIO
    NEO::EncodeMath<FamilyType>::addition(*cmdListStream, mathAluDstAddressMmio,
                                          mathAluOffsetMmio, mathAluDstAddressMmio);

    // store new dst address lower dword of SDI - patching SDI command with new dst address
    NEO::EncodeStoreMMIO<FamilyType>::encode(*cmdListStream,
                                             sdiDwordDstAddressMmio,
                                             modifySdiDstAddress,
                                             false, nullptr, false);

    // make sure all operations are complete before the next loop iteration
    NEO::PipeControlArgs pipeCtrlArgs = {};
    NEO::MemorySynchronizationCommands<FamilyType>::addSingleBarrier(*cmdListStream, pipeCtrlArgs);

    // add a jump to the startLoopBufferAddress
    NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferStart(cmdListStream, startLoopBufferAddress, false, false, false);

    // end of loop here
    uint64_t endLoopBufferAddress = cmdListStream->getCurrentGpuAddressPosition();
    // now we know end loop address, we now where to jump with predicate bb_start
    NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferStart(bbStartEndLoop, endLoopBufferAddress, false, false, true);

    // disable predicate after exiting the loop
    NEO::EncodeMiPredicate<FamilyType>::encode(*cmdListStream, NEO::MiPredicateType::disable);

    // enable prefetching in arb check
    NEO::EncodeMiArbCheck<FamilyType>::program(*cmdListStream, false);

    // store contents of counter and dst address MMIOs to testMemory
    auto testGpuAddress = reinterpret_cast<uint64_t>(testMemory);
    NEO::EncodeStoreMMIO<FamilyType>::encode(*cmdListStream,
                                             sdiDwordDstAddressMmio,
                                             testGpuAddress,
                                             false, nullptr, false);

    NEO::EncodeStoreMMIO<FamilyType>::encode(*cmdListStream,
                                             sdiCounterMmio,
                                             testGpuAddress + 8,
                                             false, nullptr, false);
    // close command list
    commandList->close();

    // execute command list
    auto cmdListHandle = commandList->toHandle();
    returnValue = pCmdq->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = pCmdq->synchronize(std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_TRUE(csr->expectMemory(dstMemory, srcMemory, size, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));
    EXPECT_TRUE(csr->expectMemory(testMemory, refMemory, sizeof(uint32_t), AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));
    EXPECT_TRUE(csr->expectMemory(ptrOffset(testMemory, 8), ptrOffset(refMemory, 8), sizeof(uint32_t), AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));

    returnValue = zeMemFree(contextHandle, srcMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemFree(contextHandle, dstMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemFree(contextHandle, testMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = zeMemFree(contextHandle, refMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

} // namespace ult
} // namespace L0
