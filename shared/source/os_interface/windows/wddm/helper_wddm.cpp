/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

NTSTATUS Wddm::createNTHandle(const D3DKMT_HANDLE *resourceHandle, HANDLE *ntHandle) {
    OBJECT_ATTRIBUTES objAttr = {};
    objAttr.Length = sizeof(OBJECT_ATTRIBUTES);

    return getGdi()->shareObjects(1, resourceHandle, &objAttr, SHARED_ALLOCATION_WRITE, ntHandle);
}

bool Wddm::getReadOnlyFlagValue(const void *cpuPtr) const {
    return !isAligned<MemoryConstants::pageSize>(cpuPtr);
}
bool Wddm::isReadOnlyFlagFallbackSupported() const {
    return true;
}

HANDLE Wddm::getSharedHandle(const MemoryManager::OsHandleData &osHandleData) {
    HANDLE sharedNtHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(osHandleData.handle));
    if (osHandleData.parentProcessId != 0) {
        // Open the parent process handle with required access rights
        HANDLE parentProcessHandle = NEO::SysCalls::openProcess(PROCESS_DUP_HANDLE, FALSE, static_cast<DWORD>(osHandleData.parentProcessId));
        if (parentProcessHandle == nullptr) {
            DEBUG_BREAK_IF(true);
            return sharedNtHandle;
        }

        // Duplicate the handle from the parent process to the current process
        // This is necessary to ensure that the handle can be used in the current process context
        // We use GENERIC_READ | GENERIC_WRITE to ensure we can perform operations on the handle
        HANDLE duplicatedHandle = nullptr;
        BOOL duplicateResult = NEO::SysCalls::duplicateHandle(
            parentProcessHandle,
            reinterpret_cast<HANDLE>(static_cast<uintptr_t>(osHandleData.handle)),
            GetCurrentProcess(),
            &duplicatedHandle,
            GENERIC_READ | GENERIC_WRITE,
            FALSE,
            0);

        // Close the parent process handle as we no longer need it
        // The duplicated handle will be used for further operations
        NEO::SysCalls::closeHandle(parentProcessHandle);

        if (!duplicateResult) {
            DEBUG_BREAK_IF(true);
            return sharedNtHandle;
        }
        sharedNtHandle = duplicatedHandle;
    }
    return sharedNtHandle;
}

} // namespace NEO
