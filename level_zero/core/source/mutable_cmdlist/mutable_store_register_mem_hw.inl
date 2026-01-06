/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_store_register_mem_hw.h"

namespace L0::MCL {

template <typename GfxFamily>
void MutableStoreRegisterMemHw<GfxFamily>::setMemoryAddress(GpuAddress memoryAddress) {
    using MemoryAddress = typename StoreRegMem::MEMORYADDRESS;
    constexpr uint32_t memoryAddressIndex = 2;

    memoryAddress += this->offset;
    memoryAddress = (memoryAddress >> MemoryAddress::MEMORYADDRESS_BIT_SHIFT << MemoryAddress::MEMORYADDRESS_BIT_SHIFT);

    auto storeRegMemCmd = reinterpret_cast<StoreRegMem *>(storeRegMem);
    memcpy_s(&storeRegMemCmd->getRawData(memoryAddressIndex), sizeof(GpuAddress), &memoryAddress, sizeof(GpuAddress));
}

} // namespace L0::MCL
