/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/page_fault_manager/linux/cpu_page_fault_manager_linux.h"

#include "core/helpers/debug_helpers.h"

#include <sys/mman.h>

namespace NEO {
std::unique_ptr<PageFaultManager> PageFaultManager::create() {
    return std::make_unique<PageFaultManagerLinux>();
}

std::function<void(int signal, siginfo_t *info, void *context)> PageFaultManagerLinux::pageFaultHandler;

PageFaultManagerLinux::PageFaultManagerLinux() {
    pageFaultHandler = [&](int signal, siginfo_t *info, void *context) {
        if (!this->verifyPageFault(info->si_addr)) {
            previousHandler.sa_sigaction(signal, info, context);
        }
    };

    struct sigaction pageFaultManagerHandler = {};
    pageFaultManagerHandler.sa_flags = SA_SIGINFO;
    pageFaultManagerHandler.sa_sigaction = pageFaultHandlerWrapper;
    auto retVal = sigaction(SIGSEGV, &pageFaultManagerHandler, &previousHandler);
    UNRECOVERABLE_IF(retVal != 0);
}

PageFaultManagerLinux::~PageFaultManagerLinux() {
    auto retVal = sigaction(SIGSEGV, &previousHandler, nullptr);
    UNRECOVERABLE_IF(retVal != 0);
}

void PageFaultManagerLinux::pageFaultHandlerWrapper(int signal, siginfo_t *info, void *context) {
    pageFaultHandler(signal, info, context);
}

void PageFaultManagerLinux::allowCPUMemoryAccess(void *ptr, size_t size) {
    auto retVal = mprotect(ptr, size, PROT_READ | PROT_WRITE);
    UNRECOVERABLE_IF(retVal != 0);
}

void PageFaultManagerLinux::protectCPUMemoryAccess(void *ptr, size_t size) {
    auto retVal = mprotect(ptr, size, PROT_NONE);
    UNRECOVERABLE_IF(retVal != 0);
}
} // namespace NEO
