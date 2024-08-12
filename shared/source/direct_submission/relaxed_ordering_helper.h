/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_alu_helper.h"

namespace NEO {
class CommandStreamReceiver;

namespace RelaxedOrderingHelper {
bool isRelaxedOrderingDispatchAllowed(const CommandStreamReceiver &csr, uint32_t numWaitEvents);

static constexpr uint32_t queueSizeMultiplier = 4;
static constexpr uint32_t maxQueueSize = 16;

template <typename GfxFamily>
void encodeRegistersBeforeDependencyCheckers(LinearStream &cmdStream, bool isBcs) {
    // Indirect BB_START operates only on GPR_0
    EncodeSetMMIO<GfxFamily>::encodeREG(cmdStream, RegisterOffsets::csGprR0, RegisterOffsets::csGprR4, isBcs);
    EncodeSetMMIO<GfxFamily>::encodeREG(cmdStream, RegisterOffsets::csGprR0 + 4, RegisterOffsets::csGprR4 + 4, isBcs);
}

template <typename GfxFamily>
constexpr size_t getQueueSizeLimitValueOffset() {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_REG = typename GfxFamily::MI_LOAD_REGISTER_REG;

    constexpr size_t lriSize = sizeof(MI_LOAD_REGISTER_IMM);

    static_assert(lriSize == (3 * sizeof(uint32_t)));

    return (sizeof(MI_LOAD_REGISTER_REG) + sizeof(MI_LOAD_REGISTER_IMM) + (lriSize - sizeof(uint32_t)));
}

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
struct StaticSchedulerSizeAndOffsetSection {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_REG = typename GfxFamily::MI_LOAD_REGISTER_REG;
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    static constexpr uint64_t initSectionSize = EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataRegBatchBufferStart(false) + (6 * sizeof(MI_LOAD_REGISTER_IMM)) +
                                                EncodeMiPredicate<GfxFamily>::getCmdSize() + (2 * sizeof(MI_LOAD_REGISTER_REG));

    static constexpr uint64_t loopStartSectionStart = initSectionSize;
    static constexpr uint64_t loopStartSectionSize = (4 * sizeof(MI_LOAD_REGISTER_IMM)) + EncodeAluHelper<GfxFamily, 10>::getCmdsSize() + sizeof(MI_BATCH_BUFFER_START) +
                                                     EncodeMiPredicate<GfxFamily>::getCmdSize();

    static constexpr uint64_t removeTaskSectionStart = loopStartSectionStart + loopStartSectionSize;
    static constexpr uint64_t removeStartSectionSize = (2 * EncodeMathMMIO<GfxFamily>::getCmdSizeForIncrementOrDecrement()) + EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataRegBatchBufferStart(false) +
                                                       (4 * sizeof(MI_LOAD_REGISTER_IMM)) + EncodeAluHelper<GfxFamily, 14>::getCmdsSize() + EncodeMiPredicate<GfxFamily>::getCmdSize() +
                                                       (2 * sizeof(MI_LOAD_REGISTER_REG));

    static constexpr uint64_t tasksListLoopCheckSectionStart = removeTaskSectionStart + removeStartSectionSize;
    static constexpr uint64_t tasksListLoopCheckSectionSize = EncodeMathMMIO<GfxFamily>::getCmdSizeForIncrementOrDecrement() + EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalRegRegBatchBufferStart() +
                                                              (2 * sizeof(MI_LOAD_REGISTER_IMM)) + EncodeMiPredicate<GfxFamily>::getCmdSize();

    static constexpr uint64_t drainRequestSectionStart = tasksListLoopCheckSectionStart + tasksListLoopCheckSectionSize;
    static constexpr uint64_t drainRequestSectionSize = sizeof(typename GfxFamily::MI_ARB_CHECK) + (2 * EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataRegBatchBufferStart(false));

    static constexpr uint64_t schedulerLoopCheckSectionStart = drainRequestSectionStart + drainRequestSectionSize;
    static constexpr uint64_t schedulerLoopCheckSectionSize = (2 * sizeof(MI_LOAD_REGISTER_IMM)) + EncodeAluHelper<GfxFamily, 4>::getCmdsSize() +
                                                              EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalRegMemBatchBufferStart() + sizeof(MI_BATCH_BUFFER_START);

    static constexpr uint64_t totalSize = schedulerLoopCheckSectionStart + schedulerLoopCheckSectionSize;
};

template <typename GfxFamily>
struct DynamicSchedulerSizeAndOffsetSection {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    static constexpr uint64_t initSectionSize = (3 * sizeof(MI_LOAD_REGISTER_IMM)) + sizeof(MI_BATCH_BUFFER_START);

    static constexpr uint64_t semaphoreSectionStart = initSectionSize;
    static constexpr uint64_t semaphoreSectionSize = EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait() + EncodeMiPredicate<GfxFamily>::getCmdSize();

    static constexpr uint64_t endSectionStart = semaphoreSectionStart + semaphoreSectionSize;
    static constexpr uint64_t endSectionSize = sizeof(MI_LOAD_REGISTER_IMM) + EncodeMiPredicate<GfxFamily>::getCmdSize();

    static constexpr uint64_t totalSize = endSectionStart + endSectionSize;
};

} // namespace RelaxedOrderingHelper
} // namespace NEO