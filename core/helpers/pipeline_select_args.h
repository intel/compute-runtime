/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstdint>

namespace NEO {
struct PipelineSelectArgs {
    bool specialPipelineSelectMode = false;
    bool mediaSamplerRequired = false;
};
} // namespace NEO
