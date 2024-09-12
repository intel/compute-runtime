/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>

enum class InternalMemoryType : uint32_t {
    notSpecified = 0b0,
    svm = 0b1,
    deviceUnifiedMemory = 0b10,
    hostUnifiedMemory = 0b100,
    sharedUnifiedMemory = 0b1000,
    reservedDeviceMemory = 0b10000,
    reservedHostMemory = 0b100000
};

enum class InternalIpcMemoryType : uint32_t {
    deviceUnifiedMemory = 0,
    hostUnifiedMemory = 1,
    deviceVirtualAddress = 2
};

enum class TransferType : uint32_t {
    unknown = 0,

    hostNonUsmToHostUsm = 1,
    hostNonUsmToDeviceUsm = 2,
    hostNonUsmToSharedUsm = 3,
    hostNonUsmToHostNonUsm = 4,

    hostUsmToHostUsm = 5,
    hostUsmToDeviceUsm = 6,
    hostUsmToSharedUsm = 7,
    hostUsmToHostNonUsm = 8,

    deviceUsmToHostUsm = 9,
    deviceUsmToDeviceUsm = 10,
    deviceUsmToSharedUsm = 11,
    deviceUsmToHostNonUsm = 12,

    sharedUsmToHostUsm = 13,
    sharedUsmToDeviceUsm = 14,
    sharedUsmToSharedUsm = 15,
    sharedUsmToHostNonUsm = 16
};

struct UnifiedMemoryControls {
    uint32_t generateMask();
    bool indirectDeviceAllocationsAllowed = false;
    bool indirectHostAllocationsAllowed = false;
    bool indirectSharedAllocationsAllowed = false;
};
