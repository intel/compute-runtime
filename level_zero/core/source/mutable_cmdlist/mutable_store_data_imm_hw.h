/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/mutable_cmdlist/mutable_store_data_imm.h"

namespace L0::MCL {

template <typename GfxFamily>
struct MutableStoreDataImmHw : public MutableStoreDataImm {
    using StoreDataImm = typename GfxFamily::MI_STORE_DATA_IMM;

    MutableStoreDataImmHw(void *storeDataImm, size_t offset, bool workloadPartition)
        : storeDataImm(storeDataImm),
          offset(offset),
          workloadPartition(workloadPartition) {}
    ~MutableStoreDataImmHw() override {}

    void setAddress(GpuAddress address) override;
    void noop() override;
    void restoreWithAddress(GpuAddress address) override;

  protected:
    void *storeDataImm;
    size_t offset;

    bool workloadPartition;

  private:
    static GpuAddress commandAddressRange;
};

} // namespace L0::MCL
