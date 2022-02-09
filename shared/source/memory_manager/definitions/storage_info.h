/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/app_resource_defines.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/constants.h"

#include <cstdint>

namespace NEO {
struct StorageInfo {
    StorageInfo() = default;
    StorageInfo(DeviceBitfield memoryBanks, DeviceBitfield pageTablesVisibility)
        : memoryBanks(memoryBanks), pageTablesVisibility(pageTablesVisibility){};
    uint32_t getNumBanks() const;
    DeviceBitfield memoryBanks;
    DeviceBitfield pageTablesVisibility;
    DeviceBitfield subDeviceBitfield;
    bool cloningOfPageTables = true;
    bool tileInstanced = false;
    bool multiStorage = false;
    ColouringPolicy colouringPolicy = ColouringPolicy::DeviceCountBased;
    size_t colouringGranularity = MemoryConstants::pageSize64k;
    bool readOnlyMultiStorage = false;
    bool cpuVisibleSegment = false;
    bool isLockable = false;
    bool localOnlyRequired = false;
    bool systemMemoryPlacement = true;
    char resourceTag[AppResourceDefines::maxStrLen + 1] = "";
    uint32_t getMemoryBanks() const { return static_cast<uint32_t>(memoryBanks.to_ulong()); }
    uint32_t getTotalBanksCnt() const { return Math::log2(getMemoryBanks()) + 1; }
};
} // namespace NEO
