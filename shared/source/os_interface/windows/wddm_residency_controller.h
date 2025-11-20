/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/residency_container.h"
#include "shared/source/os_interface/windows/windows_defs.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/source/utilities/spinlock.h"

#include <atomic>
#include <mutex>

struct _D3DKMT_TRIMNOTIFICATION;
typedef _D3DKMT_TRIMNOTIFICATION D3DKMT_TRIMNOTIFICATION;
struct D3DDDI_TRIMRESIDENCYSET_FLAGS;

namespace NEO {

class GraphicsAllocation;
class WddmAllocation;
class Wddm;
class CommandStreamReceiver;
class OsContextWin;

class WddmResidencyController {
  public:
    WddmResidencyController(Wddm &wddm);
    MOCKABLE_VIRTUAL ~WddmResidencyController() = default;

    static void APIENTRY trimCallback(_Inout_ D3DKMT_TRIMNOTIFICATION *trimNotification);

    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<SpinLock> acquireLock();
    [[nodiscard]] std::unique_lock<SpinLock> acquireTrimCallbackLock();

    void registerCallback();
    void unregisterCallback();

    void trimResidency(const D3DDDI_TRIMRESIDENCYSET_FLAGS &flags, uint64_t bytes);
    MOCKABLE_VIRTUAL bool trimResidencyToBudget(uint64_t bytes);

    bool isMemoryBudgetExhausted() const { return memoryBudgetExhausted; }
    void setMemoryBudgetExhausted() { memoryBudgetExhausted = true; }

    bool makeResidentResidencyAllocations(ResidencyContainer &allocationsForResidency, bool &requiresBlockingResidencyHandling, CommandStreamReceiver *csr);

    bool isInitialized() const;

    void removeAllocation(ResidencyContainer &container, GraphicsAllocation *gfxAllocation);

    ResidencyContainer &getEvictionAllocations() {
        return this->evictionAllocations;
    }

  protected:
    size_t fillHandlesContainer(ResidencyContainer &allocationsForResidency, bool &requiresBlockingResidencyHandling, OsContextWin &osContext);

    SpinLock lock;
    SpinLock trimCallbackLock;

    Wddm &wddm;
    VOID *trimCallbackHandle = nullptr;

    bool memoryBudgetExhausted = false;

    ResidencyContainer evictionAllocations;

    ResidencyContainer backupResidencyContainer;    // Stores allocations which should be resident
    std::vector<D3DKMT_HANDLE> handlesForResidency; // Stores D3DKMT handles of allocations which are not yet resident
};
} // namespace NEO
