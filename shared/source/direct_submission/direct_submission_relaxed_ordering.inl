/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw.h"

namespace NEO {
template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchStaticRelaxedOrderingScheduler() {
    LinearStream schedulerCmdStream(this->relaxedOrderingSchedulerAllocation);
    uint64_t schedulerStartAddress = schedulerCmdStream.getGpuBase();
    uint64_t deferredTasksListGpuVa = deferredTasksListAllocation->getGpuAddress();

    uint64_t loopSectionStartAddress = schedulerStartAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::loopStartSectionStart;

    const uint32_t miMathMocs = this->rootDeviceEnvironment.getGmmHelper()->getL3EnabledMOCS();

    constexpr bool isBcs = Dispatcher::isCopy();

    // 1. Init section
    {
        EncodeMiPredicate<GfxFamily>::encode(schedulerCmdStream, MiPredicateType::disable);

        EncodeSetMMIO<GfxFamily>::encodeREG(schedulerCmdStream, RegisterOffsets::csGprR0, RegisterOffsets::csGprR9, isBcs);
        EncodeSetMMIO<GfxFamily>::encodeREG(schedulerCmdStream, RegisterOffsets::csGprR0 + 4, RegisterOffsets::csGprR9 + 4, isBcs);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataRegBatchBufferStart(schedulerCmdStream, 0, RegisterOffsets::csGprR1, 0, CompareOperation::equal, true, false, isBcs);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR2, 0, true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR2 + 4, 0, true, isBcs);

        uint64_t removeTaskVa = schedulerStartAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::removeTaskSectionStart;
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR3, static_cast<uint32_t>(removeTaskVa & 0xFFFF'FFFFULL), true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR3 + 4, static_cast<uint32_t>(removeTaskVa >> 32), true, isBcs);

        uint64_t walkersLoopConditionCheckVa = schedulerStartAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::tasksListLoopCheckSectionStart;
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR4, static_cast<uint32_t>(walkersLoopConditionCheckVa & 0xFFFF'FFFFULL), true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR4 + 4, static_cast<uint32_t>(walkersLoopConditionCheckVa >> 32), true, isBcs);
    }

    // 2. Dispatch task section (loop start)
    {
        UNRECOVERABLE_IF(schedulerCmdStream.getUsed() != RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::loopStartSectionStart);

        EncodeMiPredicate<GfxFamily>::encode(schedulerCmdStream, MiPredicateType::disable);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR6, 8, true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR6 + 4, 0, true, isBcs);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR8, static_cast<uint32_t>(deferredTasksListGpuVa & 0xFFFF'FFFFULL), true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR8 + 4, static_cast<uint32_t>(deferredTasksListGpuVa >> 32), true, isBcs);

        EncodeAluHelper<GfxFamily, 10> aluHelper({{
            {AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr2},
            {AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr6},
            {AluRegisters::opcodeShl, AluRegisters::opcodeNone, AluRegisters::opcodeNone},
            {AluRegisters::opcodeStore, AluRegisters::gpr7, AluRegisters::accu},
            {AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr7},
            {AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr8},
            {AluRegisters::opcodeAdd, AluRegisters::opcodeNone, AluRegisters::opcodeNone},
            {AluRegisters::opcodeStore, AluRegisters::gpr6, AluRegisters::accu},
            {AluRegisters::opcodeLoadind, AluRegisters::gpr0, AluRegisters::accu},
            {AluRegisters::opcodeFenceRd, AluRegisters::opcodeNone, AluRegisters::opcodeNone},
        }});

        aluHelper.setMocs(miMathMocs);
        aluHelper.copyToCmdStream(schedulerCmdStream);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&schedulerCmdStream, 0, false, true, false);
    }

    // 3. Remove task section
    {
        UNRECOVERABLE_IF(schedulerCmdStream.getUsed() != RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::removeTaskSectionStart);

        EncodeMiPredicate<GfxFamily>::encode(schedulerCmdStream, MiPredicateType::disable);

        EncodeMathMMIO<GfxFamily>::encodeDecrement(schedulerCmdStream, AluRegisters::gpr1, isBcs);
        EncodeMathMMIO<GfxFamily>::encodeDecrement(schedulerCmdStream, AluRegisters::gpr2, isBcs);

        EncodeSetMMIO<GfxFamily>::encodeREG(schedulerCmdStream, RegisterOffsets::csGprR0, RegisterOffsets::csGprR9, isBcs);
        EncodeSetMMIO<GfxFamily>::encodeREG(schedulerCmdStream, RegisterOffsets::csGprR0 + 4, RegisterOffsets::csGprR9 + 4, isBcs);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataRegBatchBufferStart(schedulerCmdStream, 0, RegisterOffsets::csGprR1, 0, CompareOperation::equal, true, false, isBcs);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR7, 8, true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR7 + 4, 0, true, isBcs);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR8, static_cast<uint32_t>(deferredTasksListGpuVa & 0xFFFF'FFFFULL), true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR8 + 4, static_cast<uint32_t>(deferredTasksListGpuVa >> 32), true, isBcs);

        EncodeAluHelper<GfxFamily, 14> aluHelper({{
            {AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr1},
            {AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr7},
            {AluRegisters::opcodeShl, AluRegisters::opcodeNone, AluRegisters::opcodeNone},
            {AluRegisters::opcodeStore, AluRegisters::gpr7, AluRegisters::accu},
            {AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr7},
            {AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr8},
            {AluRegisters::opcodeAdd, AluRegisters::opcodeNone, AluRegisters::opcodeNone},
            {AluRegisters::opcodeLoadind, AluRegisters::gpr7, AluRegisters::accu},
            {AluRegisters::opcodeFenceRd, AluRegisters::opcodeNone, AluRegisters::opcodeNone},
            {AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr6},
            {AluRegisters::opcodeLoad0, AluRegisters::srcb, AluRegisters::opcodeNone},
            {AluRegisters::opcodeAdd, AluRegisters::opcodeNone, AluRegisters::opcodeNone},
            {AluRegisters::opcodeStoreind, AluRegisters::accu, AluRegisters::gpr7},
            {AluRegisters::opcodeFenceWr, AluRegisters::opcodeNone, AluRegisters::opcodeNone},
        }});

        aluHelper.setMocs(miMathMocs);
        aluHelper.copyToCmdStream(schedulerCmdStream);
    }

    // 4. List loop check section
    {
        UNRECOVERABLE_IF(schedulerCmdStream.getUsed() != RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::tasksListLoopCheckSectionStart);

        EncodeMiPredicate<GfxFamily>::encode(schedulerCmdStream, MiPredicateType::disable);

        EncodeMathMMIO<GfxFamily>::encodeIncrement(schedulerCmdStream, AluRegisters::gpr2, isBcs);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalRegRegBatchBufferStart(
            schedulerCmdStream,
            loopSectionStartAddress,
            AluRegisters::gpr1, AluRegisters::gpr2, CompareOperation::notEqual, false, isBcs);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR2, 0, true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR2 + 4, 0, true, isBcs);
    }

    // 5. Drain request section
    {
        UNRECOVERABLE_IF(schedulerCmdStream.getUsed() != RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::drainRequestSectionStart);

        EncodeMiArbCheck<GfxFamily>::program(schedulerCmdStream, std::nullopt);

        if (debugManager.flags.DirectSubmissionRelaxedOrderingQueueSizeLimit.get() != -1) {
            currentRelaxedOrderingQueueSize = static_cast<uint32_t>(debugManager.flags.DirectSubmissionRelaxedOrderingQueueSizeLimit.get());
        }

        this->relaxedOrderingQueueSizeLimitValueVa = schedulerCmdStream.getCurrentGpuAddressPosition() + RelaxedOrderingHelper::getQueueSizeLimitValueOffset<GfxFamily>();

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataRegBatchBufferStart(
            schedulerCmdStream,
            loopSectionStartAddress,
            RegisterOffsets::csGprR1, currentRelaxedOrderingQueueSize, CompareOperation::greaterOrEqual, false, false, isBcs);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataRegBatchBufferStart(
            schedulerCmdStream,
            loopSectionStartAddress,
            RegisterOffsets::csGprR5, 1, CompareOperation::equal, false, false, isBcs);
    }

    // 6. Scheduler loop check section
    {
        UNRECOVERABLE_IF(schedulerCmdStream.getUsed() != RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::schedulerLoopCheckSectionStart);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR10, static_cast<uint32_t>(RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::semaphoreSectionSize), true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR10 + 4, 0, true, isBcs);

        EncodeAluHelper<GfxFamily, 4> aluHelper({{
            {AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr9},
            {AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr10},
            {AluRegisters::opcodeAdd, AluRegisters::opcodeNone, AluRegisters::opcodeNone},
            {AluRegisters::opcodeStore, AluRegisters::gpr0, AluRegisters::accu},
        }});
        aluHelper.setMocs(miMathMocs);
        aluHelper.copyToCmdStream(schedulerCmdStream);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalRegMemBatchBufferStart(schedulerCmdStream, 0, semaphoreGpuVa, RegisterOffsets::csGprR11, CompareOperation::greaterOrEqual, true, isBcs);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&schedulerCmdStream, schedulerStartAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::loopStartSectionStart,
                                                                        false, false, false);
    }

    UNRECOVERABLE_IF(schedulerCmdStream.getUsed() != RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchRelaxedOrderingSchedulerSection(uint32_t value) {
    LinearStream schedulerCmdStream(this->preinitializedRelaxedOrderingScheduler.get(), RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);

    // 1. Init section

    uint64_t schedulerStartVa = ringCommandStream.getCurrentGpuAddressPosition();

    uint64_t semaphoreSectionVa = schedulerStartVa + RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::semaphoreSectionStart;

    constexpr bool isBcs = Dispatcher::isCopy();
    LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR11, value, true, isBcs);
    LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR9, static_cast<uint32_t>(semaphoreSectionVa & 0xFFFF'FFFFULL), true, isBcs);
    LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR9 + 4, static_cast<uint32_t>(semaphoreSectionVa >> 32), true, isBcs);

    schedulerCmdStream.getSpace(sizeof(typename GfxFamily::MI_BATCH_BUFFER_START)); // skip patching

    // 2. Semaphore section
    {
        using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

        schedulerCmdStream.getSpace(EncodeMiPredicate<GfxFamily>::getCmdSize()); // skip patching

        EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(schedulerCmdStream, semaphoreGpuVa, value,
                                                              COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, false, false, false, false, nullptr);
    }

    // skip patching End section

    auto dst = ringCommandStream.getSpace(RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);
    memcpy_s(dst, RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize,
             this->preinitializedRelaxedOrderingScheduler.get(), RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::updateRelaxedOrderingQueueSize(uint32_t newSize) {
    this->currentRelaxedOrderingQueueSize = newSize;

    EncodeStoreMemory<GfxFamily>::programStoreDataImm(this->ringCommandStream, this->relaxedOrderingQueueSizeLimitValueVa,
                                                      this->currentRelaxedOrderingQueueSize, 0, false, false,
                                                      nullptr);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchRelaxedOrderingQueueStall() {
    LinearStream bbStartStream(ringCommandStream.getSpace(EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataRegBatchBufferStart(false)),
                               EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataRegBatchBufferStart(false));

    constexpr bool isBcs = Dispatcher::isCopy();
    LriHelper<GfxFamily>::program(&ringCommandStream, RegisterOffsets::csGprR5, 1, true, isBcs);
    dispatchSemaphoreSection(currentQueueWorkCount);

    // patch conditional bb_start with current GPU address
    EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataRegBatchBufferStart(bbStartStream, ringCommandStream.getCurrentGpuAddressPosition(),
                                                                                      RegisterOffsets::csGprR1, 0, CompareOperation::equal, false, false, isBcs);

    relaxedOrderingSchedulerRequired = false;
}

template <typename GfxFamily, typename Dispatcher>
size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeDispatchRelaxedOrderingQueueStall() {
    return getSizeSemaphoreSection(true) + sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM) +
           EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataRegBatchBufferStart(false);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchRelaxedOrderingReturnPtrRegs(LinearStream &cmdStream, uint64_t returnPtr) {

    constexpr bool isBcs = Dispatcher::isCopy();
    LriHelper<GfxFamily>::program(&cmdStream, RegisterOffsets::csGprR4, static_cast<uint32_t>(returnPtr & 0xFFFF'FFFFULL), true, isBcs);
    LriHelper<GfxFamily>::program(&cmdStream, RegisterOffsets::csGprR4 + 4, static_cast<uint32_t>(returnPtr >> 32), true, isBcs);

    uint64_t returnPtrAfterTaskStoreSection = returnPtr;

    returnPtrAfterTaskStoreSection += RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>();

    LriHelper<GfxFamily>::program(&cmdStream, RegisterOffsets::csGprR3, static_cast<uint32_t>(returnPtrAfterTaskStoreSection & 0xFFFF'FFFFULL), true, isBcs);
    LriHelper<GfxFamily>::program(&cmdStream, RegisterOffsets::csGprR3 + 4, static_cast<uint32_t>(returnPtrAfterTaskStoreSection >> 32), true, isBcs);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::initRelaxedOrderingRegisters() {

    constexpr bool isBcs = Dispatcher::isCopy();
    LriHelper<GfxFamily>::program(&ringCommandStream, RegisterOffsets::csGprR1, 0, true, isBcs);
    LriHelper<GfxFamily>::program(&ringCommandStream, RegisterOffsets::csGprR1 + 4, 0, true, isBcs);
    LriHelper<GfxFamily>::program(&ringCommandStream, RegisterOffsets::csGprR5, 0, true, isBcs);
    LriHelper<GfxFamily>::program(&ringCommandStream, RegisterOffsets::csGprR5 + 4, 0, true, isBcs);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::preinitializeRelaxedOrderingSections() {
    // Task store section
    preinitializedTaskStoreSection = std::make_unique<uint8_t[]>(RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>());

    LinearStream stream(preinitializedTaskStoreSection.get(), RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>());

    EncodeMiPredicate<GfxFamily>::encode(stream, MiPredicateType::disable);

    uint64_t deferredTasksListGpuVa = deferredTasksListAllocation->getGpuAddress();

    constexpr bool isBcs = Dispatcher::isCopy();
    LriHelper<GfxFamily>::program(&stream, RegisterOffsets::csGprR6, static_cast<uint32_t>(deferredTasksListGpuVa & 0xFFFF'FFFFULL), true, isBcs);
    LriHelper<GfxFamily>::program(&stream, RegisterOffsets::csGprR6 + 4, static_cast<uint32_t>(deferredTasksListGpuVa >> 32), true, isBcs);

    // Task start VA
    LriHelper<GfxFamily>::program(&stream, RegisterOffsets::csGprR7, 0, true, isBcs);
    LriHelper<GfxFamily>::program(&stream, RegisterOffsets::csGprR7 + 4, 0, true, isBcs);

    // Shift by 8 = multiply by 256. Address must by 64b aligned (shift by 6), but SHL accepts only 1, 2, 4, 8, 16 and 32
    LriHelper<GfxFamily>::program(&stream, RegisterOffsets::csGprR8, 8, true, isBcs);
    LriHelper<GfxFamily>::program(&stream, RegisterOffsets::csGprR8 + 4, 0, true, isBcs);

    const uint32_t miMathMocs = this->rootDeviceEnvironment.getGmmHelper()->getL3EnabledMOCS();

    EncodeAluHelper<GfxFamily, 9> aluHelper({{
        {AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr1},
        {AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr8},
        {AluRegisters::opcodeShl, AluRegisters::opcodeNone, AluRegisters::opcodeNone},
        {AluRegisters::opcodeStore, AluRegisters::gpr8, AluRegisters::accu},
        {AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr8},
        {AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr6},
        {AluRegisters::opcodeAdd, AluRegisters::opcodeNone, AluRegisters::opcodeNone},
        {AluRegisters::opcodeStoreind, AluRegisters::accu, AluRegisters::gpr7},
        {AluRegisters::opcodeFenceWr, AluRegisters::opcodeNone, AluRegisters::opcodeNone},
    }});
    aluHelper.setMocs(miMathMocs);
    aluHelper.copyToCmdStream(stream);

    EncodeMathMMIO<GfxFamily>::encodeIncrement(stream, AluRegisters::gpr1, isBcs);

    UNRECOVERABLE_IF(stream.getUsed() != RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>());

    // Scheduler section
    preinitializedRelaxedOrderingScheduler = std::make_unique<uint8_t[]>(RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);
    LinearStream schedulerStream(preinitializedRelaxedOrderingScheduler.get(), RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);

    uint64_t schedulerStartAddress = relaxedOrderingSchedulerAllocation->getGpuAddress();

    // 1. Init section
    LriHelper<GfxFamily>::program(&schedulerStream, RegisterOffsets::csGprR11, 0, true, isBcs);
    LriHelper<GfxFamily>::program(&schedulerStream, RegisterOffsets::csGprR9, 0, true, isBcs);
    LriHelper<GfxFamily>::program(&schedulerStream, RegisterOffsets::csGprR9 + 4, 0, true, isBcs);
    EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&schedulerStream, schedulerStartAddress, false, false, false);

    // 2. Semaphore section
    {
        using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

        EncodeMiPredicate<GfxFamily>::encode(schedulerStream, MiPredicateType::disable);

        EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(schedulerStream, 0, 0, COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, false, false, false, false, nullptr);
    }

    // 3. End section
    {
        EncodeMiPredicate<GfxFamily>::encode(schedulerStream, MiPredicateType::disable);

        LriHelper<GfxFamily>::program(&schedulerStream, RegisterOffsets::csGprR5, 0, true, isBcs);
    }

    UNRECOVERABLE_IF(schedulerStream.getUsed() != RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchTaskStoreSection(uint64_t taskStartSectionVa) {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    constexpr size_t patchOffset = EncodeMiPredicate<GfxFamily>::getCmdSize() + (2 * sizeof(MI_LOAD_REGISTER_IMM));

    auto lri = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(preinitializedTaskStoreSection.get(), patchOffset));

    lri->setDataDword(static_cast<uint32_t>(taskStartSectionVa & 0xFFFF'FFFFULL));
    lri++;
    lri->setDataDword(static_cast<uint32_t>(taskStartSectionVa >> 32));

    auto dst = ringCommandStream.getSpace(RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>());
    memcpy_s(dst, RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>(), preinitializedTaskStoreSection.get(), RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>());
}
} // namespace NEO