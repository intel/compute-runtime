/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait_hw.h"

namespace L0::MCL {

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
void MutableSemaphoreWaitHw<GfxFamily>::setSemaphoreValue(uint64_t value) {
    auto semWaitCmd = reinterpret_cast<SemaphoreWait *>(semWait);
    semWaitCmd->setSemaphoreDataDword(static_cast<uint32_t>(value));
}

} // namespace L0::MCL
