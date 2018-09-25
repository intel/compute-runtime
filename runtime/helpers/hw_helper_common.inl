/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/memory_manager/memory_constants.h"

namespace OCLRT {
template <typename Family>
void HwHelperHw<Family>::setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) {
    coherencyFlag = true;
}

template <typename Family>
void HwHelperHw<Family>::adjustDefaultEngineType(HardwareInfo *pHwInfo) {
}

template <typename Family>
bool HwHelperHw<Family>::isLocalMemoryEnabled(const HardwareInfo &hwInfo) {
    return false;
}

template <typename Family>
void HwHelperHw<Family>::setupHardwareCapabilities(HardwareCapabilities *caps, const HardwareInfo &hwInfo) {
    caps->image3DMaxHeight = 16384;
    caps->image3DMaxWidth = 16384;
    //With statefull messages we have an allocation cap of 4GB
    //Reason to subtract 8KB is that driver may pad the buffer with addition pages for over fetching..
    caps->maxMemAllocSize = (4ULL * MemoryConstants::gigaByte) - (8ULL * MemoryConstants::kiloByte);
    caps->isStatelesToStatefullWithOffsetSupported = true;
    caps->localMemorySupported = isLocalMemoryEnabled(hwInfo);
}

template <typename Family>
uint32_t HwHelperHw<Family>::getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const {
    return pHwInfo->pSysInfo->MaxSubSlicesSupported * pHwInfo->pSysInfo->MaxEuPerSubSlice *
           pHwInfo->pSysInfo->ThreadCount / pHwInfo->pSysInfo->EUCount;
}

template <typename Family>
SipKernelType HwHelperHw<Family>::getSipKernelType(bool debuggingActive) {
    if (!debuggingActive) {
        return SipKernelType::Csr;
    }
    return SipKernelType::DbgCsr;
}

template <typename Family>
uint32_t HwHelperHw<Family>::getConfigureAddressSpaceMode() {
    return 0u;
}

template <typename Family>
bool HwHelperHw<Family>::setupPreemptionRegisters(HardwareInfo *pHwInfo, bool enable) {
    return false;
}

template <typename Family>
size_t HwHelperHw<Family>::getMaxBarrierRegisterPerSlice() const {
    return 32;
}

template <typename Family>
const AubMemDump::LrcaHelper &HwHelperHw<Family>::getCsTraits(EngineType engineType) const {
    return *AUBFamilyMapper<Family>::csTraits[engineType];
}
} // namespace OCLRT
