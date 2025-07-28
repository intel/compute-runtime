/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
struct PipelineSelectArgs {
    bool systolicPipelineSelectMode = false;
    bool is3DPipelineRequired = false;
    bool systolicPipelineSelectSupport = false;
};
} // namespace NEO
