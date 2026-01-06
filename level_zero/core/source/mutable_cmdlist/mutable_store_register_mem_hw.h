/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/mutable_cmdlist/mutable_store_register_mem.h"

namespace L0::MCL {

template <typename GfxFamily>
struct MutableStoreRegisterMemHw : public MutableStoreRegisterMem {
    using StoreRegMem = typename GfxFamily::MI_STORE_REGISTER_MEM;

    MutableStoreRegisterMemHw(void *storeRegMem, size_t offset) : storeRegMem(storeRegMem), offset(offset) {}
    ~MutableStoreRegisterMemHw() override {}

    void setMemoryAddress(GpuAddress memoryAddress) override;

  protected:
    void *storeRegMem;
    size_t offset;
};

} // namespace L0::MCL
