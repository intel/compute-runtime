/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

using UnifiedSharingMemoryProperties = uint64_t;

enum class UnifiedSharingContextType {
    DeviceHandle = 0x300B,
    DeviceGroup = 0x300C
};

enum class UnifiedSharingHandleType {
    LinuxFd = 1,
    Win32Shared = 2,
    Win32Nt = 3
};

struct UnifiedSharingMemoryDescription {
    UnifiedSharingHandleType type;
    void *handle;
    unsigned long long size;
};

} // namespace NEO
