/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"

namespace L0::MCL {

struct MutableLoadRegisterImm {
    MutableLoadRegisterImm(size_t inOrderPatchListIndex)
        : inOrderPatchListIndex(inOrderPatchListIndex) {}
    virtual ~MutableLoadRegisterImm() {}

    virtual void noop() = 0;
    virtual void restore() = 0;
    virtual void setValue(uint32_t value) = 0;

    size_t getInOrderPatchListIndex() const {
        return inOrderPatchListIndex;
    }

  protected:
    size_t inOrderPatchListIndex;
};

} // namespace L0::MCL
