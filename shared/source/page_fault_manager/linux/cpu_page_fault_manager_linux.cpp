/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/linux/cpu_page_fault_manager_linux.h"

#include "shared/source/helpers/debug_helpers.h"

#include <dirent.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

namespace NEO {
std::unique_ptr<PageFaultManager> PageFaultManager::create() {
    return std::make_unique<PageFaultManagerLinux>();
}

std::function<void(int signal, siginfo_t *info, void *context)> PageFaultManagerLinux::pageFaultHandler;

PageFaultManagerLinux::PageFaultManagerLinux() {
    pageFaultHandler = [&](int signal, siginfo_t *info, void *context) {
        if (signal == SIGUSR1) {
            this->waitForCopy();
        } else if (!this->verifyPageFault(info->si_addr)) {
            callPreviousHandler(signal, info, context);
        }
    };

    struct sigaction pageFaultManagerHandler = {};
    pageFaultManagerHandler.sa_flags = SA_SIGINFO;
    pageFaultManagerHandler.sa_sigaction = pageFaultHandlerWrapper;

    auto retVal = sigaction(SIGSEGV, &pageFaultManagerHandler, &previousPageFaultHandler);
    UNRECOVERABLE_IF(retVal != 0);

    retVal = sigaction(SIGUSR1, &pageFaultManagerHandler, &previousUserSignalHandler);
    UNRECOVERABLE_IF(retVal != 0);
}

PageFaultManagerLinux::~PageFaultManagerLinux() {
    if (!previousHandlerRestored) {
        auto retVal = sigaction(SIGSEGV, &previousPageFaultHandler, nullptr);
        UNRECOVERABLE_IF(retVal != 0);

        retVal = sigaction(SIGUSR1, &previousUserSignalHandler, nullptr);
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

/* This function is a WA for USM issue in multithreaded environment
   While handling page fault, before copy starts, user signal (SIGUSR1)
   is broadcasted to ensure that every thread received signal and is
   stucked on PageFaultHandler's mutex before copy from GPU to CPU proceeds. */
void PageFaultManagerLinux::broadcastWaitSignal() {
    auto selfThreadId = syscall(__NR_gettid);

    auto procDir = opendir("/proc/self/task");
    UNRECOVERABLE_IF(!procDir);

    struct dirent *dirEntry;
    while ((dirEntry = readdir(procDir)) != NULL) {
        if (dirEntry->d_name[0] == '.') {
            continue;
        }

        int threadId = atoi(dirEntry->d_name);
        if (threadId == selfThreadId) {
            continue;
        }

        sendSignalToThread(threadId);
    }

    closedir(procDir);
}

void PageFaultManagerLinux::sendSignalToThread(int threadId) {
    syscall(SYS_tkill, threadId, SIGUSR1);
}

} // namespace NEO
