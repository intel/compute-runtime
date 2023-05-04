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
    SHARED_UNIFIED_MEMORY = 0b1000,
    RESERVED_DEVICE_MEMORY = 0b10000
};

enum class InternalIpcMemoryType : uint32_t {
    IPC_DEVICE_UNIFIED_MEMORY = 0,
    IPC_HOST_UNIFIED_MEMORY = 1
};

enum TransferType : uint32_t {
    TRANSFER_TYPE_UNKNOWN = 0,

    HOST_NON_USM_TO_HOST_USM = 1,
    HOST_NON_USM_TO_DEVICE_USM = 2,
    HOST_NON_USM_TO_SHARED_USM = 3,
    HOST_NON_USM_TO_HOST_NON_USM = 4,

    HOST_USM_TO_HOST_USM = 5,
    HOST_USM_TO_DEVICE_USM = 6,
    HOST_USM_TO_SHARED_USM = 7,
    HOST_USM_TO_HOST_NON_USM = 8,

    DEVICE_USM_TO_HOST_USM = 9,
    DEVICE_USM_TO_DEVICE_USM = 10,
    DEVICE_USM_TO_SHARED_USM = 11,
    DEVICE_USM_TO_HOST_NON_USM = 12,

    SHARED_USM_TO_HOST_USM = 13,
    SHARED_USM_TO_DEVICE_USM = 14,
    SHARED_USM_TO_SHARED_USM = 15,
    SHARED_USM_TO_HOST_NON_USM = 16
};

struct UnifiedMemoryControls {
    uint32_t generateMask();
    bool indirectDeviceAllocationsAllowed = false;
    bool indirectHostAllocationsAllowed = false;
    bool indirectSharedAllocationsAllowed = false;
};
