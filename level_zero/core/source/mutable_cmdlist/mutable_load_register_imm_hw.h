/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/mutable_cmdlist/mutable_load_register_imm.h"

namespace L0::MCL {

template <typename GfxFamily>
struct MutableLoadRegisterImmHw : public MutableLoadRegisterImm {
    using LoadRegisterImm = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    MutableLoadRegisterImmHw(void *loadRegImm, uint32_t registerAddress, size_t inOrderPatchListIndex)
        : MutableLoadRegisterImm(inOrderPatchListIndex),
          loadRegImm(loadRegImm),
          registerAddress(registerAddress) {}

    ~MutableLoadRegisterImmHw() override {}

    void noop() override;
    void restore() override;
    void setValue(uint32_t value) override;

  protected:
    void *loadRegImm;
    uint32_t registerAddress;
};

} // namespace L0::MCL
