/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

enum class LocalMemoryAccessMode {
    defaultMode = 0,
    cpuAccessAllowed = 1,
    cpuAccessDisallowed = 3
};

} // namespace NEO
