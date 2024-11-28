/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/app_resource_defines.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/constants.h"

#include <cstdint>

namespace NEO {
struct StorageInfo {
    StorageInfo();
    uint32_t getNumBanks() const;
    uint32_t getMemoryBanks() const { return static_cast<uint32_t>(memoryBanks.to_ulong()); }
    uint32_t getTotalBanksCnt() const;

    size_t colouringGranularity = MemoryConstants::pageSize64k;
    DeviceBitfield memoryBanks{};
    DeviceBitfield pageTablesVisibility{};
    DeviceBitfield subDeviceBitfield{};
    ColouringPolicy colouringPolicy = ColouringPolicy::deviceCountBased;
    uint32_t numOfChunks = 0;
    bool cloningOfPageTables = true;
    bool tileInstanced = false;
    bool multiStorage = false;
    bool cpuVisibleSegment = false;
    bool isLockable = false;
    bool localOnlyRequired = false;
    bool systemMemoryPlacement = true;
    bool systemMemoryForced = false;
    char resourceTag[AppResourceDefines::maxStrLen + 1] = "";
    bool isChunked = false;
};
} // namespace NEO
