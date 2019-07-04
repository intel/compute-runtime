/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/page_fault_manager/windows/cpu_page_fault_manager_windows.h"

#include "runtime/helpers/debug_helpers.h"

namespace NEO {
std::unique_ptr<PageFaultManager> PageFaultManager::create() {
    return std::make_unique<PageFaultManagerWindows>();
}

std::function<LONG(struct _EXCEPTION_POINTERS *exceptionInfo)> PageFaultManagerWindows::pageFaultHandler;

PageFaultManagerWindows::PageFaultManagerWindows() {
    pageFaultHandler = [&](struct _EXCEPTION_POINTERS *exceptionInfo) {
        if (exceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && !this->verifyPageFault(reinterpret_cast<void *>(exceptionInfo->ExceptionRecord->ExceptionInformation[1]))) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    };

    previousHandler = AddVectoredExceptionHandler(1, pageFaultHandlerWrapper);
}

PageFaultManagerWindows::~PageFaultManagerWindows() {
    RemoveVectoredExceptionHandler(previousHandler);
}

LONG PageFaultManagerWindows::pageFaultHandlerWrapper(_EXCEPTION_POINTERS *exceptionInfo) {
    return pageFaultHandler(exceptionInfo);
}

void PageFaultManagerWindows::allowCPUMemoryAccess(void *ptr, size_t size) {
    DWORD previousState;
    auto retVal = VirtualProtect(ptr, size, PAGE_READWRITE, &previousState);
    UNRECOVERABLE_IF(!retVal);
}

void PageFaultManagerWindows::protectCPUMemoryAccess(void *ptr, size_t size) {
    DWORD previousState;
    auto retVal = VirtualProtect(ptr, size, PAGE_NOACCESS, &previousState);
    UNRECOVERABLE_IF(!retVal);
}
} // namespace NEO
