/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
struct PipelineSelectArgs {
    bool systolicPipelineSelectMode = false;
    bool mediaSamplerRequired = false;
    bool is3DPipelineRequired = false;
    bool systolicPipelineSelectSupport = false;
};
} // namespace NEO
