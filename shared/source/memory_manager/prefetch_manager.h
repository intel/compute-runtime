/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/spinlock.h"

#include <memory>
#include <vector>

namespace NEO {
struct SvmAllocationData;

class CommandStreamReceiver;
class Device;
class SVMAllocsManager;

struct PrefetchContext {
    std::vector<const void *> allocations;
    SpinLock lock;
};

class PrefetchManager : public NonCopyableOrMovableClass {
  public:
    static std::unique_ptr<PrefetchManager> create();

    virtual ~PrefetchManager() = default;

    void insertAllocation(PrefetchContext &context, const void *usmPtr, SvmAllocationData &allocData);

    MOCKABLE_VIRTUAL void migrateAllocationsToGpu(PrefetchContext &context, SVMAllocsManager &unifiedMemoryManager, Device &device, CommandStreamReceiver &csr);

    MOCKABLE_VIRTUAL void removeAllocations(PrefetchContext &context);
};

} // namespace NEO
