/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>

enum InternalMemoryType : uint32_t {
    NOT_SPECIFIED = 0b0,
    SVM = 0b1,
    DEVICE_UNIFIED_MEMORY = 0b10,
    HOST_UNIFIED_MEMORY = 0b100,
    SHARED_UNIFIED_MEMORY = 0b1000
};

enum TransferType : uint32_t {
    HOST_NON_USM_TO_HOST_USM = 0,
    HOST_NON_USM_TO_DEVICE_USM = 1,
    HOST_NON_USM_TO_HOST_NON_USM = 2,

    HOST_USM_TO_HOST_USM = 3,
    HOST_USM_TO_DEVICE_USM = 4,
    HOST_USM_TO_HOST_NON_USM = 5,

    DEVICE_USM_TO_HOST_USM = 6,
    DEVICE_USM_TO_DEVICE_USM = 7,
    DEVICE_USM_TO_HOST_NON_USM = 8
};

struct UnifiedMemoryControls {
    uint32_t generateMask();
    bool indirectDeviceAllocationsAllowed = false;
    bool indirectHostAllocationsAllowed = false;
    bool indirectSharedAllocationsAllowed = false;
};
