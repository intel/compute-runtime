/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

enum AppendOperations {
    none = 0,
    kernel = 1u,
    nonKernel = 2u,
    cmdList = 3u
};

} // namespace NEO
