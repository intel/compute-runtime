/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct CsrSizeRequestFlags {
    bool preemptionRequestChanged = false;
    bool hasSharedHandles = false;
    bool systolicPipelineSelectMode = false;
    bool activePartitionsChanged = false;
};

} // namespace NEO
