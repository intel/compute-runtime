/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_mapper_base.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

class AubHelper : public NonCopyableOrMovableClass {
  public:
    static bool isOneTimeAubWritableAllocationType(const AllocationType &type) {
        switch (type) {
        case AllocationType::PIPE:
        case AllocationType::CONSTANT_SURFACE:
        case AllocationType::GLOBAL_SURFACE:
        case AllocationType::KERNEL_ISA:
        case AllocationType::KERNEL_ISA_INTERNAL:
        case AllocationType::PRIVATE_SURFACE:
        case AllocationType::SCRATCH_SURFACE:
        case AllocationType::WORK_PARTITION_SURFACE:
        case AllocationType::BUFFER:
        case AllocationType::BUFFER_HOST_MEMORY:
        case AllocationType::IMAGE:
        case AllocationType::TIMESTAMP_PACKET_TAG_BUFFER:
        case AllocationType::EXTERNAL_HOST_PTR:
        case AllocationType::MAP_ALLOCATION:
        case AllocationType::SVM_GPU:
            return true;
        default:
            return false;
        }
    }

    static uint64_t getTotalMemBankSize();
    static int getMemTrace(uint64_t pdEntryBits);
    static uint64_t getPTEntryBits(uint64_t pdEntryBits);
    static uint32_t getMemType(uint32_t addressSpace);
    static uint64_t getPerTileLocalMemorySize(const HardwareInfo *pHwInfo);
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
