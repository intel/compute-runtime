/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
struct PipelineSelectArgs {
    bool specialPipelineSelectMode = false;
    bool mediaSamplerRequired = false;
    bool is3DPipelineRequired = false;
};
} // namespace NEO
