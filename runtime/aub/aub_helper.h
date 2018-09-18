/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/properties_helper.h"
#include "runtime/memory_manager/graphics_allocation.h"

namespace OCLRT {

class AubHelper : public NonCopyableOrMovableClass {
  public:
    static bool isOneTimeAubWritableAllocationType(const GraphicsAllocation::AllocationType &type) {
        switch (type) {
        case GraphicsAllocation::AllocationType::BUFFER:
        case GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY:
        case GraphicsAllocation::AllocationType::BUFFER_COMPRESSED:
        case GraphicsAllocation::AllocationType::IMAGE:
            return true;
        default:
            return false;
        }
    }
    static int getMemTrace(uint64_t pdEntryBits);
    static uint64_t getPTEntryBits(uint64_t pdEntryBits);

    virtual int getDataHintForPml4Entry() const = 0;
    virtual int getDataHintForPdpEntry() const = 0;
    virtual int getDataHintForPdEntry() const = 0;
    virtual int getDataHintForPtEntry() const = 0;

    virtual int getMemTraceForPml4Entry() const = 0;
    virtual int getMemTraceForPdpEntry() const = 0;
    virtual int getMemTraceForPdEntry() const = 0;
    virtual int getMemTraceForPtEntry() const = 0;
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

} // namespace OCLRT
