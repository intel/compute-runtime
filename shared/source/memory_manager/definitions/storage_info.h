/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/app_resource_defines.h"
#include "shared/source/helpers/common_types.h"

#include <cstdint>

namespace NEO {
struct StorageInfo {
    StorageInfo() = default;
    StorageInfo(DeviceBitfield memoryBanks, DeviceBitfield pageTablesVisibility)
        : memoryBanks(memoryBanks), pageTablesVisibility(pageTablesVisibility){};
    uint32_t getNumBanks() const;
    DeviceBitfield memoryBanks;
    DeviceBitfield pageTablesVisibility;
    bool cloningOfPageTables = true;
    bool tileInstanced = false;
    bool multiStorage = false;
    bool readOnlyMultiStorage = false;
    bool cpuVisibleSegment = false;
    bool isLockable = false;
    bool localOnlyRequired = false;
    char resourceTag[AppResourceDefines::maxStrLen + 1] = "";
    uint32_t getMemoryBanks() const { return static_cast<uint32_t>(memoryBanks.to_ulong()); }
};
} // namespace NEO
