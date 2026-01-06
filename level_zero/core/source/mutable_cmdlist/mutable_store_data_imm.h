/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"

namespace L0::MCL {

struct MutableStoreDataImm {
    virtual ~MutableStoreDataImm() {}

    virtual void setAddress(GpuAddress address) = 0;
    virtual void noop() = 0;
    virtual void restoreWithAddress(GpuAddress address) = 0;
};

} // namespace L0::MCL
