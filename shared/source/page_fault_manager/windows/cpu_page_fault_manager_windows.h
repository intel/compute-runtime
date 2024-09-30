/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

#include <functional>

namespace NEO {
class PageFaultManagerWindows : public PageFaultManager {
  public:
    PageFaultManagerWindows();
    ~PageFaultManagerWindows() override;

    static LONG NTAPI pageFaultHandlerWrapper(struct _EXCEPTION_POINTERS *exceptionInfo);

  protected:
    void allowCPUMemoryAccess(void *ptr, size_t size) override;
    void protectCPUMemoryAccess(void *ptr, size_t size) override;

    void evictMemoryAfterImplCopy(GraphicsAllocation *allocation, Device *device) override;
    void allowCPUMemoryEvictionImpl(bool evict, void *ptr, CommandStreamReceiver &csr, OSInterface *osInterface) override;

    bool checkFaultHandlerFromPageFaultManager() override;
    void registerFaultHandler() override;

    static std::function<LONG(struct _EXCEPTION_POINTERS *exceptionInfo)> pageFaultHandler;
    PVOID previousHandler;
};

} // namespace NEO
