/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

#include <cstddef>
#include <memory>
#include <variant>
#include <vector>

namespace NEO {
class CommandStreamReceiver;
}

namespace L0 {
struct CommandList;
struct Event;
struct EventPool;

} // namespace L0

namespace BcsSplitParams {
static constexpr size_t csrContainerSize = 12;

using CsrContainer = StackVec<NEO::CommandStreamReceiver *, csrContainerSize>;
using CmdListsForSplitContainer = StackVec<L0::CommandList *, csrContainerSize>;

struct MemCopy {
    void *dst = nullptr;
    const void *src = nullptr;
};

struct RegionCopy {
    // originXParams
    uint32_t dst = 0;
    uint32_t src = 0;
};

using CopyParams = std::variant<MemCopy, RegionCopy>;

struct SplitEventPackage {
    L0::Event *marker = nullptr;
    L0::Event *barrier = nullptr;
    StackVec<L0::Event *, csrContainerSize> subcopyEvents;
    bool reservedForRecordedCmdList = false;
};

enum class BcsSplitMode : uint8_t {
    disabled,
    immediate,
    recorded
};

struct EventsResources {
    std::vector<L0::EventPool *> pools;
    std::vector<std::unique_ptr<SplitEventPackage>> packages;
    std::vector<void *> allocsForAggregatedEvents;
    size_t currentAggregatedAllocOffset = 0;
    size_t createdFromLatestPool = 0u;
};
} // namespace BcsSplitParams
