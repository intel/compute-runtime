/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ptr_math.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait_hw.h"

namespace L0::MCL {

template <typename GfxFamily>
GpuAddress MutableSemaphoreWaitHw<GfxFamily>::commandAddressRange = maxNBitValue(64);

template <typename GfxFamily>
void MutableSemaphoreWaitHw<GfxFamily>::setSemaphoreAddress(GpuAddress semaphoreAddress) {
    using SemaphoreAddress = typename SemaphoreWait::SEMAPHOREADDRESS;
    constexpr uint32_t semaphoreAddressIndex = 2;

    semaphoreAddress &= MutableSemaphoreWaitHw<GfxFamily>::commandAddressRange;
    semaphoreAddress += this->offset;
    semaphoreAddress = (semaphoreAddress >> SemaphoreAddress::SEMAPHOREADDRESS_BIT_SHIFT << SemaphoreAddress::SEMAPHOREADDRESS_BIT_SHIFT);

    auto semWaitCmd = reinterpret_cast<SemaphoreWait *>(semWait);
    memcpy_s(&semWaitCmd->getRawData(semaphoreAddressIndex), sizeof(GpuAddress), &semaphoreAddress, sizeof(GpuAddress));
}

template <typename GfxFamily>
void MutableSemaphoreWaitHw<GfxFamily>::noop() {
    memset(semWait, 0, sizeof(SemaphoreWait));
}

template <typename GfxFamily>
void MutableSemaphoreWaitHw<GfxFamily>::restoreWithSemaphoreAddress(GpuAddress semaphoreAddress) {
    using CompareOperation = typename SemaphoreWait::COMPARE_OPERATION;

    semaphoreAddress &= MutableSemaphoreWaitHw<GfxFamily>::commandAddressRange;
    semaphoreAddress += this->offset;

    if (type == Type::regularEventWait || type == Type::cbEventTimestampSyncWait) {
        NEO::EncodeSemaphore<GfxFamily>::programMiSemaphoreWait(reinterpret_cast<SemaphoreWait *>(semWait),
                                                                semaphoreAddress,
                                                                Event::STATE_CLEARED,
                                                                CompareOperation::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                                false, true, false, false, false);
    } else if (type == Type::cbEventWait) {
        NEO::EncodeSemaphore<GfxFamily>::programMiSemaphoreWait(reinterpret_cast<SemaphoreWait *>(semWait),
                                                                semaphoreAddress,
                                                                0,
                                                                CompareOperation::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
                                                                false, true, this->qwordDataIndirect, this->qwordDataIndirect, false);
    }
}

template <typename GfxFamily>
void MutableSemaphoreWaitHw<GfxFamily>::setSemaphoreValue(uint64_t value) {
    auto semWaitCmd = reinterpret_cast<SemaphoreWait *>(semWait);
    semWaitCmd->setSemaphoreDataDword(static_cast<uint32_t>(value));
}

} // namespace L0::MCL
