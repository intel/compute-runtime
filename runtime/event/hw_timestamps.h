/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cinttypes>

namespace OCLRT {

struct HwTimeStamps {
    void initialize() {
        GlobalStartTS = 0;
        ContextStartTS = 0;
        GlobalEndTS = 0;
        ContextEndTS = 0;
        GlobalCompleteTS = 0;
        ContextCompleteTS = 0;
    }
    bool canBeReleased() const { return true; }
    uint64_t GlobalStartTS;
    uint64_t ContextStartTS;
    uint64_t GlobalEndTS;
    uint64_t ContextEndTS;
    uint64_t GlobalCompleteTS;
    uint64_t ContextCompleteTS;
};
} // namespace OCLRT
