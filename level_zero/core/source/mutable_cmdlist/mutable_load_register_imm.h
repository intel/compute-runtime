/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"

namespace L0::MCL {

struct MutableLoadRegisterImm {
    virtual ~MutableLoadRegisterImm() {}

    virtual void noop() = 0;
    virtual void restore() = 0;
    virtual void setValue(uint32_t value) = 0;
};

} // namespace L0::MCL
