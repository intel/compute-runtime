/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

#include <csignal>
#include <functional>

namespace NEO {
class PageFaultManagerLinux : public PageFaultManager {
  public:
    PageFaultManagerLinux();
    ~PageFaultManagerLinux() override;

    static void pageFaultHandlerWrapper(int signal, siginfo_t *info, void *context);

  protected:
    void allowCPUMemoryAccess(void *ptr, size_t size) override;
    void protectCPUMemoryAccess(void *ptr, size_t size) override;

    void callPreviousHandler(int signal, siginfo_t *info, void *context);
    bool previousHandlerRestored = false;

    static std::function<void(int signal, siginfo_t *info, void *context)> pageFaultHandler;
    struct sigaction previousHandler = {};
};
} // namespace NEO
