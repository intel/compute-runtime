/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

#include <csignal>
#include <functional>
#include <vector>

namespace NEO {
class PageFaultManagerLinux : public virtual CpuPageFaultManager {
  public:
    PageFaultManagerLinux();
    ~PageFaultManagerLinux() override;

    static void pageFaultHandlerWrapper(int signal, siginfo_t *info, void *context);

  protected:
    void allowCPUMemoryAccess(void *ptr, size_t size) override;
    void protectCPUMemoryAccess(void *ptr, size_t size) override;
    void protectCpuMemoryFromWrites(void *ptr, size_t size) override;

    void allowCPUMemoryEvictionImpl(bool evict, void *ptr, CommandStreamReceiver &csr, OSInterface *osInterface) override;

    bool checkFaultHandlerFromPageFaultManager() override;
    void registerFaultHandler() override;

    void callPreviousHandler(int signal, siginfo_t *info, void *context);
    bool previousHandlerRestored = false;

    static std::function<void(int signal, siginfo_t *info, void *context)> pageFaultHandler;

    std::vector<struct sigaction> previousPageFaultHandlers;

    int handlerIndex = 0;
};

class CpuPageFaultManagerLinux final : public PageFaultManagerLinux {};

} // namespace NEO
