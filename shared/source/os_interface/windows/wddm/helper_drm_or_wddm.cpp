/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {
NTSTATUS Wddm::createNTHandle(const D3DKMT_HANDLE *resourceHandle, HANDLE *ntHandle) {
    auto status = getGdi()->shareObjects(1, resourceHandle, nullptr, SHARED_ALLOCATION_ALL_ACCESS, ntHandle);

    if (status == STATUS_SUCCESS && *ntHandle == nullptr) {
        // WSL represents the shared HANDLE as a Linux fd. Zero is a valid fd,
        // but WddmAllocation reserves zero for "no handle".
        // Duplicate it to a nonzero fd and release the original
        auto fd = SysCalls::fcntl(0, F_DUPFD_CLOEXEC, 1);
        SysCalls::close(0);
        if (fd < 0) {
            return STATUS_UNSUCCESSFUL;
        }
        *ntHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(fd));
    }
    return status;
}

bool Wddm::getReadOnlyFlagValue(const void *cpuPtr) const {
    return false;
}
bool Wddm::isReadOnlyFlagFallbackSupported() const {
    return false;
}

HANDLE Wddm::createMonitoredFenceKmdWaitEvent() {
    return nullptr;
}

bool Wddm::resetMonitoredFenceKmdWaitEvent(HANDLE eventHandle) {
    return false;
}

bool Wddm::waitForMonitoredFenceKmdWaitEvent(HANDLE eventHandle, uint32_t timeoutMilliseconds) {
    return false;
}

std::unique_ptr<KmdWaiter> Wddm::createMonitoredFenceKmdWaiter(const MonitoredFence &monitoredFence, uint64_t fenceValue) {
    return nullptr;
}

void Wddm::releaseUnusedKmdWaitHandles() {
}

HANDLE Wddm::getSharedHandle(const MemoryManager::OsHandleData &osHandleData) {
    HANDLE sharedNtHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(osHandleData.handle));
    return sharedNtHandle;
}
bool Wddm::isLatePreemptionStartSupported(const HardwareInfo &hwInfo) {
    if (debugManager.flags.OverrideLatePreemptionStart.get() != -1) {
        return debugManager.flags.OverrideLatePreemptionStart.get();
    }
    return false;
}
void OsContextWin::prepareLatePreemptionStart(CREATECONTEXT_PVTDATA &privateData) {
    UNRECOVERABLE_IF(true);
}
void OsContextWin::stopLatePreemptionStartWait() {
}
} // namespace NEO
