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
    static int getMemTrace(uint64_t pdEntryBits);
    static uint64_t getPTEntryBits(uint64_t pdEntryBits);
    static uint32_t getMemType(uint32_t addressSpace);
    static uint64_t getPerTileLocalMemorySize(const HardwareInfo *pHwInfo, const ReleaseHelper *releaseHelper);
    static const std::string getDeviceConfigString(const ReleaseHelper *releaseHelper, uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount);
    static MMIOList getAdditionalMmioList();
    static void setTbxConfiguration();

    virtual int getDataHintForPml4Entry() const = 0;
    virtual int getDataHintForPdpEntry() const = 0;
    virtual int getDataHintForPdEntry() const = 0;
    virtual int getDataHintForPtEntry() const = 0;

    virtual int getMemTraceForPml4Entry() const = 0;
    virtual int getMemTraceForPdpEntry() const = 0;
    virtual int getMemTraceForPdEntry() const = 0;
    virtual int getMemTraceForPtEntry() const = 0;

    static MMIOList splitMMIORegisters(const std::string &registers, char delimiter);
};

template <typename GfxFamily>
class AubHelperHw : public AubHelper {
  public:
    AubHelperHw(bool localMemoryEnabled) : localMemoryEnabled(localMemoryEnabled){};

    int getDataHintForPml4Entry() const override;
    int getDataHintForPdpEntry() const override;
    int getDataHintForPdEntry() const override;
    int getDataHintForPtEntry() const override;

    int getMemTraceForPml4Entry() const override;
    int getMemTraceForPdpEntry() const override;
    int getMemTraceForPdEntry() const override;
    int getMemTraceForPtEntry() const override;

  protected:
    bool localMemoryEnabled;
};

} // namespace NEO
