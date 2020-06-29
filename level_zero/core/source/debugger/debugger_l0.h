/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debugger/debugger.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <memory>

namespace NEO {
class Device;
class GraphicsAllocation;
} // namespace NEO

namespace L0 {
struct SbaTrackedAddresses {
    uint64_t GeneralStateBaseAddress;
    uint64_t SurfaceStateBaseAddress;
    uint64_t DynamicStateBaseAddress;
    uint64_t IndirectObjectBaseAddress;
    uint64_t InstructionBaseAddress;
    uint64_t BindlessSurfaceStateBaseAddress;
    uint64_t BindlessSamplerStateBaseAddress;
};

class DebuggerL0 : public NEO::Debugger, NEO::NonCopyableOrMovableClass {
  public:
    static std::unique_ptr<Debugger> create(NEO::Device *device);
    bool isDebuggerActive() override;

    DebuggerL0(NEO::Device *device);
    ~DebuggerL0() override;

    NEO::GraphicsAllocation *getSbaTrackingBuffer() {
        return sbaAllocation;
    }

  protected:
    NEO::Device *device = nullptr;
    NEO::GraphicsAllocation *sbaAllocation = nullptr;
};
} // namespace L0