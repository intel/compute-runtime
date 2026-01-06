/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ptr_math.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_store_data_imm_hw.h"

namespace L0::MCL {

template <typename GfxFamily>
GpuAddress MutableStoreDataImmHw<GfxFamily>::commandAddressRange = maxNBitValue(64);

template <typename GfxFamily>
void MutableStoreDataImmHw<GfxFamily>::setAddress(GpuAddress address) {
    using Address = typename StoreDataImm::ADDRESS;
    constexpr uint32_t addressLowIndex = 1;
    constexpr uint32_t addressHighIndex = 2;

    address &= MutableStoreDataImmHw<GfxFamily>::commandAddressRange;
    address += this->offset;
    address = (address >> Address::ADDRESS_BIT_SHIFT << Address::ADDRESS_BIT_SHIFT);

    auto addressLow = getLowPart(address);
    auto addressHigh = getHighPart(address);

    auto storeDataImmCmd = reinterpret_cast<StoreDataImm *>(this->storeDataImm);
    storeDataImmCmd->getRawData(addressLowIndex) = addressLow;
    storeDataImmCmd->getRawData(addressHighIndex) = addressHigh;
}

template <typename GfxFamily>
void MutableStoreDataImmHw<GfxFamily>::noop() {
    memset(this->storeDataImm, 0, sizeof(StoreDataImm));
}

template <typename GfxFamily>
void MutableStoreDataImmHw<GfxFamily>::restoreWithAddress(GpuAddress address) {
    address &= MutableStoreDataImmHw<GfxFamily>::commandAddressRange;
    address += this->offset;

    NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(reinterpret_cast<StoreDataImm *>(this->storeDataImm),
                                                           address,
                                                           Event::STATE_SIGNALED,
                                                           0,
                                                           false,
                                                           this->workloadPartition);
}

} // namespace L0::MCL
