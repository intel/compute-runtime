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

class PrefetchManager : public NonCopyableOrMovableClass {
  public:
    static std::unique_ptr<PrefetchManager> create();

    virtual ~PrefetchManager() = default;

    void insertAllocation(SvmAllocationData &svmData);

    MOCKABLE_VIRTUAL void migrateAllocationsToGpu(SVMAllocsManager &unifiedMemoryManager, Device &device);

  protected:
    std::vector<SvmAllocationData> allocations;
    SpinLock mtx;
};

} // namespace NEO
