/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_mapper_base.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include <string>

namespace NEO {

class ReleaseHelper;
struct HardwareInfo;

class AubHelper : public NonCopyableAndNonMovableClass {
  public:
    static bool isOneTimeAubWritableAllocationType(const AllocationType &type);
    static uint64_t getTotalMemBankSize(const ReleaseHelper *releaseHelper);
    static uint64_t getPTEntryBits(uint64_t pdEntryBits);
    static uint64_t getPerTileLocalMemorySize(const HardwareInfo *pHwInfo, const ReleaseHelper *releaseHelper);
    static const std::string getDeviceConfigString(const ReleaseHelper *releaseHelper, uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount);
    static MMIOList getAdditionalMmioList();
    static void setTbxConfiguration();

    static MMIOList splitMMIORegisters(const std::string &registers, char delimiter);
};

} // namespace NEO
