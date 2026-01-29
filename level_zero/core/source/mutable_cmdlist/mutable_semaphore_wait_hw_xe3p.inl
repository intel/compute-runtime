/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait_hw.h"

namespace L0::MCL {

template <>
void MutableSemaphoreWaitHw<NEO::Xe3pCoreFamily>::setSemaphoreAddress(GpuAddress semaphoreAddress) {
    semaphoreAddress &= MutableSemaphoreWaitHw<NEO::Xe3pCoreFamily>::commandAddressRange;
    semaphoreAddress += this->offset;
    if (this->useSemaphore64bCmd) {
        using SemaphoreAddress = typename SemaphoreWait::SEMAPHOREADDRESS;
        constexpr uint32_t semaphoreAddressIndex = 3;
        semaphoreAddress = (semaphoreAddress >> SemaphoreAddress::SEMAPHOREADDRESS_BIT_SHIFT << SemaphoreAddress::SEMAPHOREADDRESS_BIT_SHIFT);

        auto semWaitCmd = reinterpret_cast<SemaphoreWait *>(semWait);
        memcpy_s(&semWaitCmd->getRawData(semaphoreAddressIndex), sizeof(GpuAddress), &semaphoreAddress, sizeof(GpuAddress));
    } else {
        using SemaphoreWaitLegacy = typename NEO::Xe3pCoreFamily::MI_SEMAPHORE_WAIT_LEGACY;
        using SemaphoreAddress = typename SemaphoreWaitLegacy::SEMAPHOREADDRESS;
        constexpr uint32_t semaphoreAddressIndex = 2;
        semaphoreAddress = (semaphoreAddress >> SemaphoreAddress::SEMAPHOREADDRESS_BIT_SHIFT << SemaphoreAddress::SEMAPHOREADDRESS_BIT_SHIFT);

        auto semWaitCmd = reinterpret_cast<SemaphoreWaitLegacy *>(semWait);
        memcpy_s(&semWaitCmd->getRawData(semaphoreAddressIndex), sizeof(GpuAddress), &semaphoreAddress, sizeof(GpuAddress));
    }
}

template <>
void MutableSemaphoreWaitHw<NEO::Xe3pCoreFamily>::setSemaphoreValue(uint64_t value) {
    if (this->useSemaphore64bCmd) {
        auto semWaitCmd = reinterpret_cast<SemaphoreWait *>(semWait);
        semWaitCmd->setSemaphoreDataDword(value);
    } else {
        using SemaphoreWaitLegacy = typename NEO::Xe3pCoreFamily::MI_SEMAPHORE_WAIT_LEGACY;
        auto semWaitCmd = reinterpret_cast<SemaphoreWaitLegacy *>(semWait);
        semWaitCmd->setSemaphoreDataDword(static_cast<uint32_t>(value));
    }
}

} // namespace L0::MCL
