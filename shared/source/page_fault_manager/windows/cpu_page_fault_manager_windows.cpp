/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/windows/cpu_page_fault_manager_windows.h"

#include "shared/source/helpers/debug_helpers.h"

namespace NEO {
std::unique_ptr<PageFaultManager> PageFaultManager::create() {
    auto pageFaultManager = std::make_unique<PageFaultManagerWindows>();

    pageFaultManager->selectGpuDomainHandler();
    return pageFaultManager;
}

std::function<LONG(struct _EXCEPTION_POINTERS *exceptionInfo)> PageFaultManagerWindows::pageFaultHandler;

PageFaultManagerWindows::PageFaultManagerWindows() {
    pageFaultHandler = [this](struct _EXCEPTION_POINTERS *exceptionInfo) {
        if (exceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
            if (this->verifyPageFault(reinterpret_cast<void *>(exceptionInfo->ExceptionRecord->ExceptionInformation[1]))) {
                //this is our fault that we serviced, continue app execution
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
        //not our exception
        return EXCEPTION_CONTINUE_SEARCH;
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

void PageFaultManagerWindows::evictMemoryAfterImplCopy(GraphicsAllocation *allocation, Device *device) {}

} // namespace NEO
