/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/linux/cpu_page_fault_manager_linux.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/page_fault_manager/linux/tbx_page_fault_manager_linux.h"

#include <algorithm>
#include <sys/mman.h>

namespace NEO {
std::unique_ptr<CpuPageFaultManager> CpuPageFaultManager::create() {
    auto pageFaultManager = [&]() -> std::unique_ptr<CpuPageFaultManager> {
        if (debugManager.isTbxPageFaultManagerEnabled()) {
            return TbxPageFaultManager::create();
        }
        return std::make_unique<PageFaultManagerLinux>();
    }();

    pageFaultManager->selectGpuDomainHandler();
    return pageFaultManager;
}

std::function<void(int signal, siginfo_t *info, void *context)> PageFaultManagerLinux::pageFaultHandler = nullptr;

PageFaultManagerLinux::PageFaultManagerLinux() {
    PageFaultManagerLinux::registerFaultHandler();
    UNRECOVERABLE_IF(pageFaultHandler == nullptr);
}

PageFaultManagerLinux::~PageFaultManagerLinux() {
    if (!previousHandlerRestored) {
        auto retVal = sigaction(SIGSEGV, &previousPageFaultHandlers[0], nullptr);
        UNRECOVERABLE_IF(retVal != 0);
        previousPageFaultHandlers.clear();
    }
}

bool PageFaultManagerLinux::checkFaultHandlerFromPageFaultManager() {
    struct sigaction currentPageFaultHandler = {};
    sigaction(SIGSEGV, NULL, &currentPageFaultHandler);
    return (currentPageFaultHandler.sa_sigaction == pageFaultHandlerWrapper);
}

void PageFaultManagerLinux::registerFaultHandler() {
    struct sigaction previousPageFaultHandler = {};
    auto retVal = sigaction(SIGSEGV, nullptr, &previousPageFaultHandler);
    UNRECOVERABLE_IF(retVal != 0);

    auto compareHandler = [&ph = previousPageFaultHandler](const struct sigaction &h) -> bool {
        return (h.sa_flags & SA_SIGINFO) ? (h.sa_sigaction == ph.sa_sigaction) : (h.sa_handler == ph.sa_handler);
    };
    if (std::find_if(previousPageFaultHandlers.begin(),
                     previousPageFaultHandlers.end(),
                     compareHandler) == previousPageFaultHandlers.end()) {
        previousPageFaultHandlers.push_back(previousPageFaultHandler);
    }

    pageFaultHandler = [&](int signal, siginfo_t *info, void *context) {
        if (!this->verifyAndHandlePageFault(info->si_addr, this->handlerIndex == 0)) {
            callPreviousHandler(signal, info, context);
        }
    };

    struct sigaction pageFaultManagerHandler = {};
    pageFaultManagerHandler.sa_flags = SA_SIGINFO;
    pageFaultManagerHandler.sa_sigaction = pageFaultHandlerWrapper;

    retVal = sigaction(SIGSEGV, &pageFaultManagerHandler, &previousPageFaultHandler);
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

void PageFaultManagerLinux::protectCpuMemoryFromWrites(void *ptr, size_t size) {
    auto retVal = mprotect(ptr, size, PROT_READ);
    UNRECOVERABLE_IF(retVal != 0);
}

void PageFaultManagerLinux::callPreviousHandler(int signal, siginfo_t *info, void *context) {
    handlerIndex++;
    UNRECOVERABLE_IF(handlerIndex < 0 && handlerIndex >= static_cast<int>(previousPageFaultHandlers.size()));
    auto previousPageFaultHandler = previousPageFaultHandlers[previousPageFaultHandlers.size() - handlerIndex];
    if (previousPageFaultHandler.sa_flags & SA_SIGINFO) {
        previousPageFaultHandler.sa_sigaction(signal, info, context);
    } else {
        if (previousPageFaultHandler.sa_handler == SIG_DFL) {
            auto retVal = sigaction(SIGSEGV, &previousPageFaultHandler, nullptr);
            UNRECOVERABLE_IF(retVal != 0);
            previousHandlerRestored = true;
            previousPageFaultHandlers.clear();
        } else if (previousPageFaultHandler.sa_handler != SIG_IGN) {
            previousPageFaultHandler.sa_handler(signal);
        }
    }
    handlerIndex--;
}

void PageFaultManagerLinux::allowCPUMemoryEvictionImpl(bool evict, void *ptr, CommandStreamReceiver &csr, OSInterface *osInterface) {}

} // namespace NEO
