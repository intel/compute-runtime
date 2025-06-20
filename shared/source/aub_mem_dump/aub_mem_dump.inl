/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"

#include <algorithm>
#include <cstring>

namespace AubMemDump {

template <typename Traits>
void AubPageTableHelper32<Traits>::fixupLRC(uint8_t *pLRC) {
    uint32_t pdAddress;
    pdAddress = BaseClass::getPDEAddress(0x600) >> 32;
    *(uint32_t *)(pLRC + 0x1094) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0x600) & 0xffffffff;
    *(uint32_t *)(pLRC + 0x109c) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0x400) >> 32;
    *(uint32_t *)(pLRC + 0x10a4) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0x400) & 0xffffffff;
    *(uint32_t *)(pLRC + 0x10ac) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0x200) >> 32;
    *(uint32_t *)(pLRC + 0x10b4) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0x200) & 0xffffffff;
    *(uint32_t *)(pLRC + 0x10bc) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0) >> 32;
    *(uint32_t *)(pLRC + 0x10c4) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0) & 0xffffffff;
    *(uint32_t *)(pLRC + 0x10cc) = pdAddress;
}

template <typename Traits>
void AubPageTableHelper64<Traits>::fixupLRC(uint8_t *pLRC) {
    uint32_t pml4Address = getPML4Address(0) >> 32;
    *(uint32_t *)(pLRC + 0x10c4) = pml4Address;
    pml4Address = getPML4Address(0) & 0xffffffff;
    *(uint32_t *)(pLRC + 0x10cc) = pml4Address;
}

template <typename Traits>
uint64_t AubPageTableHelper32<Traits>::reserveAddressPPGTT(typename Traits::Stream &stream, uintptr_t gfxAddress,
                                                           size_t blockSize, uint64_t physAddress,
                                                           uint64_t additionalBits, const NEO::AubHelper &aubHelper) {
    return 0;
}

template <typename Traits>
uint64_t AubPageTableHelper64<Traits>::reserveAddressPPGTT(typename Traits::Stream &stream, uintptr_t gfxAddress,
                                                           size_t blockSize, uint64_t physAddress,
                                                           uint64_t additionalBits, const NEO::AubHelper &aubHelper) {
    return 0;
}

template <typename Traits>
void AubPageTableHelper32<Traits>::createContext(typename Traits::Stream &stream, uint32_t context) {}

template <typename Traits>
void AubPageTableHelper64<Traits>::createContext(typename Traits::Stream &stream, uint32_t context) {}

} // namespace AubMemDump
