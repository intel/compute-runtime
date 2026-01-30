/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/ipc_socket_client.h"
#include "shared/source/os_interface/linux/ipc_socket_server.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/cpuintrinsics.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"

#include <sys/mman.h>
#include <sys/prctl.h>

namespace L0 {

uint8_t ContextImp::isOpaqueHandleSupported(IpcHandleType *handleType) {
    auto rootEnv = this->driverHandle->getMemoryManager()->peekExecutionEnvironment().rootDeviceEnvironments[0].get();
    auto driverType = rootEnv->osInterface ? rootEnv->osInterface->getDriverModel()->getDriverModelType()
                                           : NEO::DriverModelType::unknown;
    if (driverType == NEO::DriverModelType::wddm) {
        *handleType = IpcHandleType::ntHandle;
        return OpaqueHandlingType::nthandle;
    } else {
        *handleType = IpcHandleType::fdHandle;
        for (auto &device : this->driverHandle->devices) {
            auto &productHelper = device->getNEODevice()->getProductHelper();
            if (!productHelper.isPidFdOrSocketForIpcSupported()) {
                return OpaqueHandlingType::none;
            }
        }
        if (NEO::debugManager.flags.ForceIpcSocketFallback.get()) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "Forcing IPC socket method instead of pidfd\n");
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
            return OpaqueHandlingType::pidfd;
        }
    }
}

bool ContextImp::isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice, bool shareableWithoutNTHandle) {
    if (exportableMemory) {
        return true;
    }
    if (neoDevice->getRootDeviceEnvironment().osInterface) {
        NEO::DriverModelType driverType = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
        if (!exportDesc && driverType == NEO::DriverModelType::wddm) {
            return true;
        }
    }

    return false;
}

void *ContextImp::getMemHandlePtr(ze_device_handle_t hDevice,
                                  uint64_t handle,
                                  NEO::AllocationType allocationType,
                                  unsigned int processId,
                                  ze_ipc_memory_flags_t flags,
                                  uint64_t cacheID) {
    L0::Device *device = L0::Device::fromHandle(hDevice);
    auto neoDevice = device->getNEODevice();
    NEO::DriverModelType driverType = NEO::DriverModelType::unknown;
    if (neoDevice->getRootDeviceEnvironment().osInterface) {
        driverType = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
    }
    bool isNTHandle = this->getDriverHandle()->getMemoryManager()->isNTHandle(NEO::toOsHandle(reinterpret_cast<void *>(handle)), device->getNEODevice()->getRootDeviceIndex());
    if (isNTHandle) {
        return this->driverHandle->importNTHandle(hDevice,
                                                  reinterpret_cast<void *>(handle),
                                                  allocationType,
                                                  processId);
    } else if (driverType == NEO::DriverModelType::drm) {
        auto neoDevice = Device::fromHandle(hDevice)->getNEODevice();

        NEO::SvmAllocationData allocDataInternal(neoDevice->getRootDeviceIndex());
        uint64_t importHandle = handle;
        uint64_t effectiveCacheID = cacheID;
        bool pidfdSuccess = false;
        bool socketFallbackSuccess = false;
        if (settings.useOpaqueHandle && processId != 0) {
            // Check cache first for opaque handles
            if (tryGetCachedImportHandle(cacheID, importHandle)) {
                pidfdSuccess = true; // Mark as successful to skip import logic
            }

            // Try pidfd approach first (unless forced to use socket fallback or already cached)
            if (!pidfdSuccess && (settings.useOpaqueHandle & OpaqueHandlingType::pidfd) && !NEO::debugManager.flags.ForceIpcSocketFallback.get()) {
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
                    } else {
                        importHandle = static_cast<uint64_t>(newfd);
                        pidfdSuccess = true;
                        // Cache the imported handle for future use
                        setCachedImportHandle(cacheID, importHandle);
                        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                     "Cached import handle %lu for cache ID %lu\n",
                                     importHandle, cacheID);
                    }
                }
            }

            // Try socket fallback if pidfd failed and socket fallback is enabled
            if (!pidfdSuccess && (settings.useOpaqueHandle == OpaqueHandlingType::sockets)) {
                pid_t exporterPid = static_cast<pid_t>(processId);
                std::string socketPath = "neo_ipc_" + std::to_string(exporterPid);

                NEO::IpcSocketClient socketClient;
                if (socketClient.connectToServer(socketPath)) {
                    int receivedFd = socketClient.requestHandle(handle);
                    if (receivedFd != -1) {
                        importHandle = static_cast<uint64_t>(receivedFd);
                        socketFallbackSuccess = true;
                        // Cache the imported handle for future use
                        setCachedImportHandle(cacheID, importHandle);
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

                // If socket fallback was attempted but failed, return nullptr
                if (!socketFallbackSuccess) {
                    PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                 "Socket fallback failed for handle %lu, returning nullptr\n", handle);
                    return nullptr;
                }
            }
            // If sockets are not enabled and pidfd failed, fall back to original handle
        }

        auto result = this->driverHandle->importFdHandle(neoDevice,
                                                         flags,
                                                         importHandle,
                                                         allocationType,
                                                         nullptr,
                                                         nullptr,
                                                         allocDataInternal);

        // Store cacheID in IPC handle tracking if opaque handles are used
        if (result && settings.useOpaqueHandle && effectiveCacheID != 0) {
            auto lock = driverHandle->lockIPCHandleMap();
            auto &ipcMap = driverHandle->getIPCHandleMap();
            auto ipcIter = ipcMap.find(importHandle);
            if (ipcIter != ipcMap.end()) {
                ipcIter->second->cacheID = effectiveCacheID;
            }
        }

        return result;
    } else {
        return nullptr;
    }
}

void ContextImp::getDataFromIpcHandle(ze_device_handle_t hDevice, const ze_ipc_mem_handle_t ipcHandle, uint64_t &handle, uint8_t &type, unsigned int &processId, uint64_t &poolOffset, uint64_t &cacheID) {
    if (settings.useOpaqueHandle) {
        const IpcOpaqueMemoryData *ipcData = reinterpret_cast<const IpcOpaqueMemoryData *>(ipcHandle.data);
        handle = static_cast<uint64_t>(ipcData->handle.fd);
        type = ipcData->memoryType;
        processId = ipcData->processId;
        poolOffset = ipcData->poolOffset;
        cacheID = ipcData->computeCacheID();
    } else {
        const IpcMemoryData *ipcData = reinterpret_cast<const IpcMemoryData *>(ipcHandle.data);
        handle = ipcData->handle;
        type = ipcData->type;
        poolOffset = ipcData->poolOffset;
        cacheID = 0;
    }
}

ze_result_t ContextImp::systemBarrier(ze_device_handle_t hDevice) {
    if (hDevice == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto device = Device::fromHandle(hDevice);
    if (!device) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto neoDevice = device->getNEODevice();

    NEO::DriverModelType driverModelType = NEO::DriverModelType::unknown;
    if (neoDevice->getRootDeviceEnvironment().osInterface) {
        driverModelType = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
    }

    if (driverModelType == NEO::DriverModelType::wddm || driverModelType == NEO::DriverModelType::unknown) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    auto *drmMemoryManager = static_cast<NEO::DrmMemoryManager *>(this->driverHandle->getMemoryManager());

    if (neoDevice->getHardwareInfo().capabilityTable.isIntegratedDevice) {
        return ZE_RESULT_SUCCESS;
    }

    void *ptr = drmMemoryManager->getDrm(neoDevice->getRootDeviceIndex()).getIoctlHelper()->pciBarrierMmap();
    if (ptr == MAP_FAILED || ptr == nullptr) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    NEO::CpuIntrinsics::sfence();
    auto pciBarrierPtr = static_cast<uint32_t *>(ptr);
    *pciBarrierPtr = 0u;
    NEO::CpuIntrinsics::sfence();

    NEO::SysCalls::munmap(ptr, MemoryConstants::pageSize);
    return ZE_RESULT_SUCCESS;
}
} // namespace L0
