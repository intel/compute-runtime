/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/spinlock.h"

#include <memory>
#include <vector>

namespace NEO {

class Device;
class SVMAllocsManager;

struct PrefetchContext {
    std::vector<SvmAllocationData> allocations;
    SpinLock lock;
};

class PrefetchManager : public NonCopyableOrMovableClass {
  public:
    static std::unique_ptr<PrefetchManager> create();

    virtual ~PrefetchManager() = default;

    void insertAllocation(PrefetchContext &context, SvmAllocationData &svmData);

    MOCKABLE_VIRTUAL void migrateAllocationsToGpu(PrefetchContext &context, SVMAllocsManager &unifiedMemoryManager, Device &device);

    MOCKABLE_VIRTUAL void removeAllocations(PrefetchContext &context);
};

} // namespace NEO
