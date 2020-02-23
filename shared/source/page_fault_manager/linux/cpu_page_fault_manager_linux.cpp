/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/linux/cpu_page_fault_manager_linux.h"

#include "shared/source/helpers/debug_helpers.h"

#include <sys/mman.h>

namespace NEO {
std::unique_ptr<PageFaultManager> PageFaultManager::create() {
    return std::make_unique<PageFaultManagerLinux>();
}

std::function<void(int signal, siginfo_t *info, void *context)> PageFaultManagerLinux::pageFaultHandler;

PageFaultManagerLinux::PageFaultManagerLinux() {
    pageFaultHandler = [&](int signal, siginfo_t *info, void *context) {
        if (!this->verifyPageFault(info->si_addr)) {
            callPreviousHandler(signal, info, context);
        }
    };

    struct sigaction pageFaultManagerHandler = {};
    pageFaultManagerHandler.sa_flags = SA_SIGINFO;
    pageFaultManagerHandler.sa_sigaction = pageFaultHandlerWrapper;
    auto retVal = sigaction(SIGSEGV, &pageFaultManagerHandler, &previousHandler);
    UNRECOVERABLE_IF(retVal != 0);
}

PageFaultManagerLinux::~PageFaultManagerLinux() {
    if (!previousHandlerRestored) {
        auto retVal = sigaction(SIGSEGV, &previousHandler, nullptr);
        UNRECOVERABLE_IF(retVal != 0);
    }
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

void PageFaultManagerLinux::callPreviousHandler(int signal, siginfo_t *info, void *context) {
    if (previousHandler.sa_flags & SA_SIGINFO) {
        previousHandler.sa_sigaction(signal, info, context);
    } else {
        if (previousHandler.sa_handler == SIG_DFL) {
            auto retVal = sigaction(SIGSEGV, &previousHandler, nullptr);
            UNRECOVERABLE_IF(retVal != 0);
            previousHandlerRestored = true;
        } else if (previousHandler.sa_handler == SIG_IGN) {
            return;
        } else {
            previousHandler.sa_handler(signal);
        }
    }
}
} // namespace NEO
