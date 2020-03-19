/*
 * Copyright (C) 2017-2020 Intel Corporation
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
