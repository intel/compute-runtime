/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/windows/cpu_page_fault_manager_windows.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/page_fault_manager/windows/tbx_page_fault_manager_windows.h"

namespace NEO {
std::unique_ptr<CpuPageFaultManager> CpuPageFaultManager::create() {
    auto pageFaultManager = [&]() -> std::unique_ptr<CpuPageFaultManager> {
        if (debugManager.isTbxPageFaultManagerEnabled()) {
            return TbxPageFaultManager::create();
        }
        return std::make_unique<CpuPageFaultManagerWindows>();
    }();

    pageFaultManager->selectGpuDomainHandler();
    return pageFaultManager;
}

std::function<LONG(struct _EXCEPTION_POINTERS *exceptionInfo)> PageFaultManagerWindows::pageFaultHandler = nullptr;

PageFaultManagerWindows::PageFaultManagerWindows() {
    PageFaultManagerWindows::registerFaultHandler();
}

PageFaultManagerWindows::~PageFaultManagerWindows() {
    RemoveVectoredExceptionHandler(previousHandler);
}

bool PageFaultManagerWindows::checkFaultHandlerFromPageFaultManager() {
    return true;
}

void PageFaultManagerWindows::registerFaultHandler() {
    pageFaultHandler = [this](struct _EXCEPTION_POINTERS *exceptionInfo) {
        if (static_cast<long>(exceptionInfo->ExceptionRecord->ExceptionCode) == EXCEPTION_ACCESS_VIOLATION) {
            if (this->verifyAndHandlePageFault(reinterpret_cast<void *>(exceptionInfo->ExceptionRecord->ExceptionInformation[1]), true)) {
                // this is our fault that we serviced, continue app execution
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
        // not our exception
        return EXCEPTION_CONTINUE_SEARCH;
    };

    previousHandler = AddVectoredExceptionHandler(1, pageFaultHandlerWrapper);
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

void PageFaultManagerWindows::protectCpuMemoryFromWrites(void *ptr, size_t size) {
    DWORD previousState;
    auto retVal = VirtualProtect(ptr, size, PAGE_READONLY, &previousState);
    UNRECOVERABLE_IF(!retVal);
}

void PageFaultManagerWindows::allowCPUMemoryEvictionImpl(bool evict, void *ptr, CommandStreamReceiver &csr, OSInterface *osInterface) {
    NEO::SvmAllocationData *allocData = this->memoryData[ptr].unifiedMemoryManager->getSVMAlloc(ptr);
    UNRECOVERABLE_IF(allocData == nullptr);

    if (osInterface) {
        auto &residencyController = static_cast<OsContextWin *>(&csr.getOsContext())->getResidencyController();

        auto lock = residencyController.acquireLock();
        auto &evictContainer = csr.getEvictionAllocations();
        auto iter = std::find(evictContainer.begin(), evictContainer.end(), allocData->cpuAllocation);
        auto allocInEvictionList = iter != evictContainer.end();
        if (evict && !allocInEvictionList) {
            evictContainer.push_back(allocData->cpuAllocation);
        } else if (!evict && allocInEvictionList) {
            evictContainer.erase(iter);
        }
    }
}

} // namespace NEO
