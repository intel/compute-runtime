/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_alu_helper.h"

namespace NEO {
namespace RelaxedOrderingHelper {

template <typename GfxFamily>
constexpr size_t getSizeTaskStoreSection() {
    return ((6 * sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM)) +
            EncodeAluHelper<GfxFamily, 9>::getCmdsSize() +
            EncodeMathMMIO<GfxFamily>::getCmdSizeForIncrementOrDecrement() +
            EncodeMiPredicate<GfxFamily>::getCmdSize());
}

template <typename GfxFamily>
constexpr size_t getSizeRegistersInit() {
    return (4 * sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM));
}

template <typename GfxFamily>
constexpr size_t getSizeReturnPtrRegs() {
    return (4 * sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM));
}

template <typename GfxFamily>
struct SchedulerSizeAndOffsetSection {
    using MI_MATH = typename GfxFamily::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    static constexpr uint64_t initSectionSize = EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataRegBatchBufferStart() + (6 * sizeof(MI_LOAD_REGISTER_IMM)) +
                                                EncodeMiPredicate<GfxFamily>::getCmdSize();

    static constexpr uint64_t loopStartSectionStart = initSectionSize;
    static constexpr uint64_t loopStartSectionSize = (4 * sizeof(MI_LOAD_REGISTER_IMM)) + EncodeAluHelper<GfxFamily, 10>::getCmdsSize() + sizeof(MI_BATCH_BUFFER_START) +
                                                     EncodeMiPredicate<GfxFamily>::getCmdSize();

    static constexpr uint64_t removeTaskSectionStart = loopStartSectionStart + loopStartSectionSize;
    static constexpr uint64_t removeStartSectionSize = (2 * EncodeMathMMIO<GfxFamily>::getCmdSizeForIncrementOrDecrement()) + EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataRegBatchBufferStart() +
                                                       (4 * sizeof(MI_LOAD_REGISTER_IMM)) + EncodeAluHelper<GfxFamily, 14>::getCmdsSize() + EncodeMiPredicate<GfxFamily>::getCmdSize();

    static constexpr uint64_t tasksListLoopCheckSectionStart = removeTaskSectionStart + removeStartSectionSize;
    static constexpr uint64_t tasksListLoopCheckSectionSize = EncodeMathMMIO<GfxFamily>::getCmdSizeForIncrementOrDecrement() + EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalRegRegBatchBufferStart() +
                                                              (2 * sizeof(MI_LOAD_REGISTER_IMM)) + EncodeMiPredicate<GfxFamily>::getCmdSize();

    static constexpr uint64_t drainRequestSectionStart = tasksListLoopCheckSectionStart + tasksListLoopCheckSectionSize;
    static constexpr uint64_t drainRequestSectionSize = sizeof(typename GfxFamily::MI_ARB_CHECK) + EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataRegBatchBufferStart();

    static constexpr uint64_t schedulerLoopCheckSectionStart = drainRequestSectionStart + drainRequestSectionSize;
    static constexpr uint64_t schedulerLoopCheckSectionSize = EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataMemBatchBufferStart() + sizeof(MI_BATCH_BUFFER_START);

    static constexpr uint64_t semaphoreSectionStart = schedulerLoopCheckSectionStart + schedulerLoopCheckSectionSize;
    static constexpr uint64_t semaphoreSectionSize = EncodeSempahore<GfxFamily>::getSizeMiSemaphoreWait() + EncodeMiPredicate<GfxFamily>::getCmdSize();

    static constexpr uint64_t endSectionStart = semaphoreSectionStart + semaphoreSectionSize;
    static constexpr uint64_t endSectionSize = sizeof(MI_LOAD_REGISTER_IMM) + EncodeMiPredicate<GfxFamily>::getCmdSize();

    static constexpr uint64_t totalSize = endSectionStart + endSectionSize;
};

} // namespace RelaxedOrderingHelper
} // namespace NEO