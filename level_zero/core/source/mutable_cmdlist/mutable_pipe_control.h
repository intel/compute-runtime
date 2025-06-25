/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"

namespace L0::MCL {

struct MutablePipeControl {
    virtual ~MutablePipeControl() {}

    virtual void setPostSyncAddress(GpuAddress postSyncAddress) = 0;
};

} // namespace L0::MCL
