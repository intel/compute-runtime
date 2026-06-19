/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/allocation_type.h"

#include "aubstream/aubstream.h"

#include <string>

namespace NEO {

class ReleaseHelper;
struct HardwareInfo;

class AubHelper : public NonCopyableAndNonMovableClass {
  public:
    static bool isOneTimeAubWritableAllocationType(const AllocationType &type);
    static uint64_t getPTEntryBits(uint64_t pdEntryBits);
    static uint64_t getPerTileLocalMemorySize(const HardwareInfo *pHwInfo, const ReleaseHelper &releaseHelper);
    static aub_stream::MMIOList getAdditionalMmioList();
    static void setTbxConfiguration();

    static aub_stream::MMIOList splitMMIORegisters(const std::string &registers, char delimiter);
};

} // namespace NEO
