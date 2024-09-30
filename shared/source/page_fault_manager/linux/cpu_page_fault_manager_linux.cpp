/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/linux/cpu_page_fault_manager_linux.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/memory_manager/memory_operations_handler.h"

#include <sys/mman.h>

namespace NEO {
std::unique_ptr<PageFaultManager> PageFaultManager::create() {
    auto pageFaultManager = std::make_unique<PageFaultManagerLinux>();

    pageFaultManager->selectGpuDomainHandler();
    return pageFaultManager;
}

std::function<void(int signal, siginfo_t *info, void *context)> PageFaultManagerLinux::pageFaultHandler = nullptr;

PageFaultManagerLinux::PageFaultManagerLinux() {
    PageFaultManagerLinux::registerFaultHandler();
    UNRECOVERABLE_IF(pageFaultHandler == nullptr);

    this->evictMemoryAfterCopy = debugManager.flags.EnableDirectSubmission.get() &&
                                 debugManager.flags.USMEvictAfterMigration.get();
}

PageFaultManagerLinux::~PageFaultManagerLinux() {
    if (!previousHandlerRestored) {
        auto retVal = sigaction(SIGSEGV, &previousPageFaultHandler, nullptr);
        UNRECOVERABLE_IF(retVal != 0);
    }
}

bool PageFaultManagerLinux::checkFaultHandlerFromPageFaultManager() {
    struct sigaction currentPageFaultHandler = {};
    sigaction(SIGSEGV, NULL, &currentPageFaultHandler);
    return (currentPageFaultHandler.sa_sigaction == pageFaultHandlerWrapper);
}

void PageFaultManagerLinux::registerFaultHandler() {
    pageFaultHandler = [&](int signal, siginfo_t *info, void *context) {
        if (!this->verifyPageFault(info->si_addr)) {
            callPreviousHandler(signal, info, context);
        }
    };

    struct sigaction pageFaultManagerHandler = {};
    pageFaultManagerHandler.sa_flags = SA_SIGINFO;
    pageFaultManagerHandler.sa_sigaction = pageFaultHandlerWrapper;

    auto retVal = sigaction(SIGSEGV, &pageFaultManagerHandler, &previousPageFaultHandler);
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

void PageFaultManagerLinux::callPreviousHandler(int signal, siginfo_t *info, void *context) {
    if (previousPageFaultHandler.sa_flags & SA_SIGINFO) {
        previousPageFaultHandler.sa_sigaction(signal, info, context);
    } else {
        if (previousPageFaultHandler.sa_handler == SIG_DFL) {
            auto retVal = sigaction(SIGSEGV, &previousPageFaultHandler, nullptr);
            UNRECOVERABLE_IF(retVal != 0);
            previousHandlerRestored = true;
        } else if (previousPageFaultHandler.sa_handler == SIG_IGN) {
            return;
        } else {
            previousPageFaultHandler.sa_handler(signal);
        }
    }
}

void PageFaultManagerLinux::evictMemoryAfterImplCopy(GraphicsAllocation *allocation, Device *device) {
    if (evictMemoryAfterCopy) {
        device->getRootDeviceEnvironment().memoryOperationsInterface->evict(device, *allocation);
    }
}

void PageFaultManagerLinux::allowCPUMemoryEvictionImpl(bool evict, void *ptr, CommandStreamReceiver &csr, OSInterface *osInterface) {}

} // namespace NEO
