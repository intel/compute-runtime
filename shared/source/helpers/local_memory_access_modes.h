/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

enum class LocalMemoryAccessMode {
    Default = 0,
    CpuAccessAllowed = 1,
    CpuAccessDisallowed = 3
};

} // namespace NEO
