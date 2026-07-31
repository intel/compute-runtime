/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/ptr_math.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait_hw.h"

namespace L0::MCL {

template <typename GfxFamily>
MutableSemaphoreWaitHw<GfxFamily>::MutableSemaphoreWaitHw(uint64_t gpuDestination, void *cmdView, void *semWait, size_t offset, Type type, bool qwordData, bool useSemaphore64bCmd)
    : MutableSemaphoreWait(gpuDestination, cmdView, sizeof(SemaphoreWait), type),
      semWait(semWait),
      offset(offset),
      qwordData(qwordData),
      useSemaphore64bCmd(useSemaphore64bCmd),
      qwordIndirect(NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(this->qwordData, this->useSemaphore64bCmd)) {}

template <typename GfxFamily>
MutableSemaphoreWaitHw<GfxFamily>::~MutableSemaphoreWaitHw() {
    if (this->commandView) {
        NEO::EncodeSemaphore<GfxFamily>::deallocateSemaphoreWaitCommand(this->commandView, this->useSemaphore64bCmd);
    }
}

template <typename GfxFamily>
GpuAddress MutableSemaphoreWaitHw<GfxFamily>::commandAddressRange = maxNBitValue(64);

template <typename GfxFamily>
void MutableSemaphoreWaitHw<GfxFamily>::noop() {
    if (this->commandView) {
        memset(this->commandView, 0, sizeof(SemaphoreWait));
    } else {
        memset(this->semWait, 0, sizeof(SemaphoreWait));
    }
}

template <typename GfxFamily>
void MutableSemaphoreWaitHw<GfxFamily>::restoreWithSemaphoreAddress(GpuAddress semaphoreAddress) {
    using CompareOperation = typename SemaphoreWait::COMPARE_OPERATION;

    constexpr bool registerPollMode = false;
    constexpr bool waitMode = true;
    constexpr bool switchOnUnsuccessful = false;

    semaphoreAddress &= MutableSemaphoreWaitHw<GfxFamily>::commandAddressRange;
    semaphoreAddress += this->offset;

    void *targetPointer = this->commandView ? this->commandView : this->semWait;

    if (type == Type::regularEventWait || type == Type::cbEventTimestampSyncWait) {
        constexpr bool useQwordData = false;
        constexpr bool indirect = false;
        NEO::EncodeSemaphore<GfxFamily>::programMiSemaphoreWait(reinterpret_cast<SemaphoreWait *>(targetPointer),
                                                                semaphoreAddress,
                                                                Event::STATE_CLEARED,
                                                                CompareOperation::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                                registerPollMode, waitMode, useQwordData, indirect, switchOnUnsuccessful, this->useSemaphore64bCmd);
    } else if (type == Type::cbEventWait || type == Type::cbEventWaitPatchPreambleCounter) {
        NEO::EncodeSemaphore<GfxFamily>::programMiSemaphoreWait(reinterpret_cast<SemaphoreWait *>(targetPointer),
                                                                semaphoreAddress,
                                                                0,
                                                                CompareOperation::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
                                                                registerPollMode, waitMode, this->qwordData, this->qwordIndirect, switchOnUnsuccessful, this->useSemaphore64bCmd);
    }
}

} // namespace L0::MCL
