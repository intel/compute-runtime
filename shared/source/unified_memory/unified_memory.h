/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdint.h>
#pragma once

enum InternalMemoryType : uint32_t {
    NOT_SPECIFIED = 0b0,
    SVM = 0b1,
    DEVICE_UNIFIED_MEMORY = 0b10,
    HOST_UNIFIED_MEMORY = 0b100,
    SHARED_UNIFIED_MEMORY = 0b1000
};

struct UnifiedMemoryControls {
    uint32_t generateMask();
    bool indirectDeviceAllocationsAllowed = false;
    bool indirectHostAllocationsAllowed = false;
    bool indirectSharedAllocationsAllowed = false;
};
