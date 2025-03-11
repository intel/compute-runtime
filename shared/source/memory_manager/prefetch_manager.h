/*
 * Copyright (C) 2022-2025 Intel Corporation
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

struct Allocation {
    const void *ptr;
    const size_t size;
};

struct PrefetchContext {
    std::vector<Allocation> allocations;
    SpinLock lock;
};

class PrefetchManager : public NonCopyableAndNonMovableClass {
  public:
    static std::unique_ptr<PrefetchManager> create();

    virtual ~PrefetchManager() = default;

    void insertAllocation(PrefetchContext &context, SVMAllocsManager &unifiedMemoryManager, Device &device, const void *usmPtr, const size_t size);

    MOCKABLE_VIRTUAL void migrateAllocationsToGpu(PrefetchContext &context, SVMAllocsManager &unifiedMemoryManager, Device &device, CommandStreamReceiver &csr);

    MOCKABLE_VIRTUAL void removeAllocations(PrefetchContext &context);
};

} // namespace NEO
