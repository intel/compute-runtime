/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

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
void HwHelperHw<Family>::setupHardwareCapabilities(HardwareCapabilities *caps) {
    caps->image3DMaxHeight = 16384;
    caps->image3DMaxWidth = 16384;
    //With statefull messages we have an allocation cap of 4GB
    //Reason to subtract 8KB is that driver may pad the buffer with addition pages for over fetching..
    caps->maxMemAllocSize = (4ULL * MemoryConstants::gigaByte) - (8ULL * MemoryConstants::kiloByte);
    caps->isStatelesToStatefullWithOffsetSupported = true;
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

} // namespace OCLRT
