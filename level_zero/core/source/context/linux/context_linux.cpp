/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/linux/ipc_socket_client.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <string>
#include <sys/prctl.h>
#include <sys/resource.h>

namespace L0 {

constexpr rlim_t maxOpaqueHandlePreallocation = 4096;

uint8_t Context::isDrmOpaqueHandleSupported(IpcHandleType *handleType) {
    *handleType = IpcHandleType::fdHandle;
    if (NEO::debugManager.flags.ForceIpcSocketFallback.get()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Forcing IPC socket fallback instead of pidfd\n");
        return OpaqueHandlingType::sockets;
    }
    if (NEO::SysCalls::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) == -1) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "prctl Syscall for PR_SET_PTRACER, PR_SET_PTRACER_ANY failed: %s\n", strerror(errno));
        if (NEO::debugManager.flags.EnableIpcSocketFallback.get()) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "pidfd unavailable, but socket fallback is enabled - opaque handles still supported\n");
            return OpaqueHandlingType::sockets;
        }
        return OpaqueHandlingType::none;
    } else {
        // Verify pidfdopen and pidfdgetfd syscalls are available
        int testPidfd = NEO::SysCalls::pidfdopen(-1, 0);
        if (testPidfd == -1 && errno == ENOSYS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "pidfd_open syscall not available: %s\n", strerror(errno));
            if (NEO::debugManager.flags.EnableIpcSocketFallback.get()) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                             "pidfd unavailable, but socket fallback is enabled - opaque handles still supported\n");
                return OpaqueHandlingType::sockets;
            }
            return OpaqueHandlingType::none;
        }
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "pidfd approach for IPC handles is supported\n");
        initOpaqueHandleResources();
        return OpaqueHandlingType::pidfd;
    }
}

Context::OpaqueHandleImportResult Context::importOpaqueHandleWithFallback(uint64_t handle,
                                                                          unsigned int processId,
                                                                          uint64_t cacheID,
                                                                          void *reservedHandleData,
                                                                          NEO::Device *neoDevice) {
    uint64_t importHandle = handle;
    bool handleRetrieved = false;
    bool socketFallbackSuccess = false;
    bool opaqueHandlesAttempted = false;

    // Check cache first for opaque handles
    if (this->driverHandle->tryGetCachedImportHandle(cacheID, importHandle)) {
        handleRetrieved = true; // Mark as successful to skip import logic
    }

    // Try pidfd approach first (unless forced to use socket fallback or already cached)
    if (!handleRetrieved && (settings.useOpaqueHandle & OpaqueHandlingType::pidfd) && !NEO::debugManager.flags.ForceIpcSocketFallback.get()) {
        pid_t exporterPid = static_cast<pid_t>(processId);
        unsigned int pidfdFlags = 0u;
        int pidfd = NEO::SysCalls::pidfdopen(exporterPid, pidfdFlags);
        if (pidfd == -1) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "pidfd_open Syscall failed: %s\n", strerror(errno));
        } else {
            unsigned int getfdFlags = 0u;
            int newfd = NEO::SysCalls::pidfdgetfd(pidfd, static_cast<int>(handle), getfdFlags);
            NEO::SysCalls::close(pidfd);
            if (newfd < 0) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "pidfd_getfd Syscall failed: %s\n", strerror(errno));
                return {0, false, false};
            } else {
                importHandle = static_cast<uint64_t>(newfd);
                handleRetrieved = true;
                opaqueHandlesAttempted = true;
                // Cache the imported handle for future use
                this->driverHandle->setCachedImportHandle(cacheID, importHandle);
            }
        }
    }

    // Try socket fallback if pidfd failed and socket fallback is enabled
    if (!handleRetrieved && (settings.useOpaqueHandle & OpaqueHandlingType::sockets)) {
        pid_t exporterPid = static_cast<pid_t>(processId);
        std::string socketPath = "neo_ipc_" + std::to_string(exporterPid);

        NEO::IpcSocketClient socketClient;
        if (socketClient.connectToServer(socketPath)) {
            int receivedFd = socketClient.requestHandle(handle);
            if (receivedFd != -1) {
                importHandle = static_cast<uint64_t>(receivedFd);
                socketFallbackSuccess = true;
                opaqueHandlesAttempted = true;
                // Cache the imported handle for future use
                this->driverHandle->setCachedImportHandle(cacheID, importHandle);
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                             "IPC socket fallback successful for handle %lu, cached as %lu\n",
                             handle, importHandle);
            } else {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                             "IPC socket fallback failed for handle %lu\n", handle);
            }
        } else {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "Failed to connect to IPC socket server at %s\n", socketPath.c_str());
        }

        // If socket fallback was attempted but failed, return failure
        if (!socketFallbackSuccess) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "Socket fallback failed for handle %lu, returning failure\n", handle);
            return {0, false, false};
        }
    }

    return {importHandle, true, opaqueHandlesAttempted};
}

void Context::getDataFromIpcHandle(ze_device_handle_t hDevice, const ze_ipc_mem_handle_t &ipcHandle, uint64_t &handle, uint8_t &type, unsigned int &processId, uint64_t &poolOffset, uint64_t &cacheID, void *&reservedHandleData, bool &compressedMemory, bool &isOpaqueHandle) {
    if (settings.useOpaqueHandle) {
        const IpcOpaqueMemoryData *ipcData = reinterpret_cast<const IpcOpaqueMemoryData *>(ipcHandle.data);
        handle = static_cast<uint64_t>(ipcData->handle.fd);
        type = ipcData->memoryType;
        processId = ipcData->processId;
        poolOffset = ipcData->poolOffset;
        cacheID = ipcData->computeCacheID();
        compressedMemory = ipcData->compressedMemory;
        uint8_t emptyReservedData[32] = {0};
        if (std::memcmp(ipcData->reservedHandleData, emptyReservedData, sizeof(emptyReservedData)) != 0) {
            reservedHandleData = const_cast<void *>(static_cast<const void *>(&ipcData->reservedHandleData));
        }
        // Check if opaqueHandle matches handle - if not, user changed it, so fallback to legacy path
        uint64_t normalizedHandle = normalizeIPCHandle(*ipcData);
        uint64_t normalizedOpaqueHandle = 0;
        switch (ipcData->type) {
        case IpcHandleType::ntHandle:
            normalizedOpaqueHandle = ipcData->opaqueHandle.reserved;
            break;
        default:
            normalizedOpaqueHandle = static_cast<uint64_t>(static_cast<int64_t>(ipcData->opaqueHandle.fd));
            break;
        }
        isOpaqueHandle = (normalizedHandle == normalizedOpaqueHandle);
        if (!isOpaqueHandle) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "IPC handle %lu does not match opaque handle %lu, falling back to legacy path. The handle was likely modified by the user.\n",
                         normalizedHandle, normalizedOpaqueHandle);
        }
    } else {
        const IpcMemoryData *ipcData = reinterpret_cast<const IpcMemoryData *>(ipcHandle.data);
        handle = ipcData->handle;
        type = ipcData->type;
        poolOffset = ipcData->poolOffset;
        processId = 0;
        cacheID = 0;
        reservedHandleData = nullptr;
        isOpaqueHandle = false;
    }
}

void Context::initOpaqueHandleResourcesImpl() {
    struct rlimit rlim;
    if (NEO::SysCalls::getrlimit(RLIMIT_NOFILE, &rlim)) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "getrlimit RLIMIT_NOFILE failed, no preallocation of fds for opaque ipc handles.\n");
        return;
    }

    // Use the minimum of ulimit and a reasonable maximum
    rlim_t availableFDs = std::min(rlim.rlim_cur / 10, maxOpaqueHandlePreallocation);
    int32_t overrideCount = NEO::debugManager.flags.IpcFdPreallocationCount.get();
    if (overrideCount >= 1 && static_cast<rlim_t>(overrideCount) <= rlim.rlim_cur) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcFdPreallocationCount override active: using %d fds (ulimit: %lld)\n",
                     overrideCount, static_cast<long long>(rlim.rlim_cur));
        availableFDs = static_cast<rlim_t>(overrideCount);
    } else {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Using default FD preallocation: %lld fds (ulimit: %lld, max: %lld)\n",
                     static_cast<long long>(availableFDs), static_cast<long long>(rlim.rlim_cur),
                     static_cast<long long>(maxOpaqueHandlePreallocation));
    }
    auto tempFds = std::make_unique<int[]>(availableFDs);

    rlim_t i;
    for (i = 0; i < availableFDs; ++i) {
        tempFds[i] = NEO::SysCalls::open("/dev/null", O_RDONLY);
        if (tempFds[i] < 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "open() failed at fd index %lld, using partial preallocation\n", static_cast<long long>(i));
            break;
        }
    }
    availableFDs = i;

    for (i = 0; i < availableFDs; ++i) {
        NEO::SysCalls::close(tempFds[i]);
    }

    PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                 "preallocation of fds for opaque ipc handles completed with %lld fds\n", static_cast<long long>(availableFDs));
}

void *Context::importHandleFromReservedHandleData(void *reservedHandleData,
                                                  uint64_t cacheID,
                                                  NEO::Device *neoDevice,
                                                  ze_ipc_memory_flags_t flags,
                                                  NEO::AllocationType allocationType,
                                                  bool isHostIpcAllocation,
                                                  bool compressedMemory,
                                                  uint64_t &importHandle,
                                                  NEO::GraphicsAllocation *&alloc) {
    int reservedHandle = this->driverHandle->getMemoryManager()->getImportHandleFromReservedHandleData(reservedHandleData, neoDevice->getRootDeviceIndex());
    if (reservedHandle != -1) {
        importHandle = static_cast<uint64_t>(reservedHandle);
        this->driverHandle->setCachedImportHandle(cacheID, importHandle);
        NEO::SvmAllocationData allocDataRetry(neoDevice->getRootDeviceIndex());
        return this->driverHandle->importFdHandle(neoDevice,
                                                  flags,
                                                  importHandle,
                                                  allocationType,
                                                  isHostIpcAllocation,
                                                  nullptr,
                                                  &alloc,
                                                  allocDataRetry,
                                                  compressedMemory);
    }
    return nullptr;
}

} // namespace L0
