/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

#include <atomic>

namespace NEO {

class PageFaultManagerWindows : public virtual CpuPageFaultManager {
  public:
    PageFaultManagerWindows();
    ~PageFaultManagerWindows() override;

    static LONG NTAPI pageFaultHandlerWrapper(struct _EXCEPTION_POINTERS *exceptionInfo);

  protected:
    void allowCPUMemoryAccess(void *ptr, size_t size) override;
    void protectCPUMemoryAccess(void *ptr, size_t size) override;
    void protectCpuMemoryFromWrites(void *ptr, size_t size) override;

    void allowCPUMemoryEvictionImpl(bool evict, void *ptr, CommandStreamReceiver &csr, OSInterface *osInterface) override;

    bool checkFaultHandlerFromPageFaultManager() override;
    void registerFaultHandler() override;

    static constinit std::atomic<PageFaultManagerWindows *> activePageFaultManager;
    PVOID previousHandler = nullptr;
};

class CpuPageFaultManagerWindows final : public PageFaultManagerWindows {};

} // namespace NEO
