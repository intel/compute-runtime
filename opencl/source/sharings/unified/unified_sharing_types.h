/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

using UnifiedSharingMemoryProperties = uint64_t;

enum class UnifiedSharingContextType {
    deviceHandle = 0x300B,
    deviceGroup = 0x300C
};

enum class UnifiedSharingHandleType {
    linuxFd = 1,
    win32Shared = 2,
    win32Nt = 3
};

struct UnifiedSharingMemoryDescription {
    UnifiedSharingHandleType type;
    void *handle;
    unsigned long long size;
};

} // namespace NEO
