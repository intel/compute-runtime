/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/linux/prelim/debug_session.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_thread.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/include/zet_intel_gpu_debug.h"
#include "level_zero/tools/source/debug/linux/prelim/drm_helper.h"
#include <level_zero/ze_api.h>

#include "common/StateSaveAreaHeader.h"

#include <algorithm>
#include <fcntl.h>

namespace L0 {

DebugSession *createDebugSessionHelper(const zet_debug_config_t &config, Device *device, int debugFd, void *params);

DebugSessionLinux::DebugSessionLinux(const zet_debug_config_t &config, Device *device, int debugFd, void *params) : DebugSessionImp(config, device), fd(debugFd) {
    ioctlHandler.reset(new IoctlHandler);

    if (params) {
        this->i915DebuggerVersion = reinterpret_cast<prelim_drm_i915_debugger_open_param *>(params)->version;
    }

    if (this->i915DebuggerVersion >= 3) {
        this->blockOnFenceMode = true;
    }

    for (size_t i = 0; i < arrayCount(euControlInterruptSeqno); i++) {
        euControlInterruptSeqno[i] = invalidHandle;
    }
};
DebugSessionLinux::~DebugSessionLinux() {
    closeAsyncThread();
    closeInternalEventsThread();
    for (auto &session : tileSessions) {
        delete session.first;
    }
    tileSessions.resize(0);
    closeFd();
}

DebugSession *DebugSession::create(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach) {
    if (device->getOsInterface().isDebugAttachAvailable()) {
        struct prelim_drm_i915_debugger_open_param open = {};
        open.pid = config.pid;
        open.events = 0;
        open.version = 0;

        auto debugFd = DrmHelper::ioctl(device, NEO::DrmIoctl::DebuggerOpen, &open);
        if (debugFd >= 0) {
            PRINT_DEBUGGER_INFO_LOG("PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN: open.pid: %d, open.events: %d, debugFd: %d\n",
                                    open.pid, open.events, debugFd);

            auto debugSession = createDebugSessionHelper(config, device, debugFd, &open);
            debugSession->setAttachMode(isRootAttach);
            result = debugSession->initialize();

            if (result != ZE_RESULT_SUCCESS) {
                debugSession->closeConnection();
                delete debugSession;
                debugSession = nullptr;
            } else {
                debugSession->startAsyncThread();
            }
            return debugSession;
        } else {
            auto reason = DrmHelper::getErrno(device);
            PRINT_DEBUGGER_ERROR_LOG("PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN failed: open.pid: %d, open.events: %d, retCode: %d, errno: %d\n",
                                     open.pid, open.events, debugFd, reason);
            result = DebugSessionLinux::translateDebuggerOpenErrno(reason);
        }
    } else {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return nullptr;
}

ze_result_t DebugSessionLinux::translateDebuggerOpenErrno(int error) {

    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    switch (error) {
    case ENODEV:
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    case EBUSY:
        result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        break;
    case EACCES:
        result = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
        break;
    }
    return result;
}

int DebugSessionLinux::ioctl(unsigned long request, void *arg) {
    return ioctlHandler->ioctl(fd, request, arg);
}

ze_result_t DebugSessionLinux::readGpuMemory(uint64_t vmHandle, char *output, size_t size, uint64_t gpuVa) {
    prelim_drm_i915_debug_vm_open vmOpen = {
        .client_handle = static_cast<decltype(prelim_drm_i915_debug_vm_open::client_handle)>(clientHandle),
        .handle = static_cast<decltype(prelim_drm_i915_debug_vm_open::handle)>(vmHandle),
        .flags = PRELIM_I915_DEBUG_VM_OPEN_READ_ONLY};

    int vmDebugFd = ioctl(PRELIM_I915_DEBUG_IOCTL_VM_OPEN, &vmOpen);
    if (vmDebugFd < 0) {
        PRINT_DEBUGGER_ERROR_LOG("PRELIM_I915_DEBUG_IOCTL_VM_OPEN failed = %d\n", vmDebugFd);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    int64_t retVal = 0;
    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
    gpuVa = gmmHelper->decanonize(gpuVa);

    if (NEO::DebugManager.flags.EnableDebuggerMmapMemoryAccess.get()) {
        uint64_t alignedMem = alignDown(gpuVa, MemoryConstants::pageSize);
        uint64_t alignedDiff = gpuVa - alignedMem;
        uint64_t alignedSize = size + alignedDiff;

        void *mappedPtr = ioctlHandler->mmap(NULL, alignedSize, PROT_READ, MAP_SHARED, vmDebugFd, alignedMem);

        if (mappedPtr == MAP_FAILED) {
            PRINT_DEBUGGER_ERROR_LOG("Reading memory failed, errno = %d\n", errno);
            retVal = -1;
        } else {
            char *realSourceVA = static_cast<char *>(mappedPtr) + alignedDiff;
            retVal = memcpy_s(output, size, static_cast<void *>(realSourceVA), size);
            ioctlHandler->munmap(mappedPtr, alignedSize);
        }
    } else {
        size_t pendingSize = size;
        uint8_t retry = 0;
        const uint8_t maxRetries = 3;
        size_t retrySize = size;
        do {
            PRINT_DEBUGGER_MEM_ACCESS_LOG("Reading (pread) memory from gpu va = %#" PRIx64 ", size = %zu\n", gpuVa, pendingSize);
            retVal = ioctlHandler->pread(vmDebugFd, output, pendingSize, gpuVa);
            output += retVal;
            gpuVa += retVal;
            pendingSize -= retVal;
            if (retVal == 0) {
                if (pendingSize < retrySize) {
                    retry = 0;
                }
                retry++;
                retrySize = pendingSize;
            }
        } while (((retVal == 0) && (retry < maxRetries)) || ((retVal > 0) && (pendingSize > 0)));

        if (retVal < 0) {
            PRINT_DEBUGGER_ERROR_LOG("Reading memory failed, errno = %d\n", errno);
        }

        retVal = pendingSize;
    }

    NEO::SysCalls::close(vmDebugFd);

    return (retVal == 0) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t DebugSessionLinux::writeGpuMemory(uint64_t vmHandle, const char *input, size_t size, uint64_t gpuVa) {
    prelim_drm_i915_debug_vm_open vmOpen = {
        .client_handle = static_cast<decltype(prelim_drm_i915_debug_vm_open::client_handle)>(clientHandle),
        .handle = static_cast<decltype(prelim_drm_i915_debug_vm_open::handle)>(vmHandle),
        .flags = PRELIM_I915_DEBUG_VM_OPEN_READ_WRITE};

    int vmDebugFd = ioctl(PRELIM_I915_DEBUG_IOCTL_VM_OPEN, &vmOpen);
    if (vmDebugFd < 0) {
        PRINT_DEBUGGER_ERROR_LOG("PRELIM_I915_DEBUG_IOCTL_VM_OPEN failed = %d\n", vmDebugFd);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    int64_t retVal = 0;
    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
    gpuVa = gmmHelper->decanonize(gpuVa);

    if (NEO::DebugManager.flags.EnableDebuggerMmapMemoryAccess.get()) {
        uint64_t alignedMem = alignDown(gpuVa, MemoryConstants::pageSize);
        uint64_t alignedDiff = gpuVa - alignedMem;
        uint64_t alignedSize = size + alignedDiff;

        void *mappedPtr = ioctlHandler->mmap(NULL, alignedSize, PROT_WRITE, MAP_SHARED, vmDebugFd, alignedMem);

        if (mappedPtr == MAP_FAILED) {
            PRINT_DEBUGGER_ERROR_LOG("Writing memory failed, errno = %d\n", errno);
            retVal = -1;
        } else {
            char *realDestVA = static_cast<char *>(mappedPtr) + alignedDiff;
            retVal = memcpy_s(static_cast<void *>(realDestVA), size, input, size);
            ioctlHandler->munmap(mappedPtr, alignedSize);
        }
    } else {
        size_t pendingSize = size;
        uint8_t retry = 0;
        const uint8_t maxRetries = 3;
        size_t retrySize = size;
        do {
            PRINT_DEBUGGER_MEM_ACCESS_LOG("Writing (pwrite) memory to gpu va = %#" PRIx64 ", size = %zu\n", gpuVa, pendingSize);
            retVal = ioctlHandler->pwrite(vmDebugFd, input, pendingSize, gpuVa);
            input += retVal;
            gpuVa += retVal;
            pendingSize -= retVal;
            if (retVal == 0) {
                if (pendingSize < retrySize) {
                    retry = 0;
                }
                retry++;
                retrySize = pendingSize;
            }
        } while (((retVal == 0) && (retry < maxRetries)) || ((retVal > 0) && (pendingSize > 0)));

        if (retVal < 0) {
            PRINT_DEBUGGER_ERROR_LOG("Writing memory failed, errno = %d\n", errno);
        }

        retVal = pendingSize;
    }

    NEO::SysCalls::close(vmDebugFd);

    return (retVal == 0) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t DebugSessionLinux::initialize() {
    struct pollfd pollFd = {
        .fd = this->fd,
        .events = POLLIN,
        .revents = 0,
    };

    auto numberOfFds = ioctlHandler->poll(&pollFd, 1, 1000);
    PRINT_DEBUGGER_INFO_LOG("initialization poll() retCode: %d\n", numberOfFds);

    if (numberOfFds <= 0) {
        return ZE_RESULT_NOT_READY;
    }

    bool isRootDevice = !connectedDevice->getNEODevice()->isSubDevice();
    if (isRootDevice && !tileAttachEnabled) {
        createEuThreads();
    }
    createTileSessionsIfEnabled();
    startInternalEventsThread();

    bool allEventsCollected = false;
    bool eventAvailable = true;
    float timeDelta = 0;
    float timeStart = clock();
    do {
        if (internalThreadHasStarted) {
            auto eventMemory = getInternalEvent();
            auto debugEvent = reinterpret_cast<prelim_drm_i915_debug_event *>(eventMemory.get());
            if (eventMemory != nullptr) {
                handleEvent(debugEvent);
                if (debugEvent->type != PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND && pendingVmBindEvents.size() > 0) {
                    processPendingVmBindEvents();
                }
                eventAvailable = true;
            } else {
                eventAvailable = false;
            }
            allEventsCollected = checkAllEventsCollected();
        } else {
            timeDelta = float(clock() - timeStart) / CLOCKS_PER_SEC;
        }
    } while ((eventAvailable && !allEventsCollected) && timeDelta < getThreadStartLimitTime());

    internalThreadHasStarted = false;

    if (clientHandleClosed == clientHandle && clientHandle != invalidClientHandle) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    if (allEventsCollected) {
        if (!readModuleDebugArea()) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    return ZE_RESULT_NOT_READY;
}

void DebugSessionLinux::createTileSessionsIfEnabled() {
    auto numTiles = connectedDevice->getNEODevice()->getNumSubDevices();
    if (numTiles > 0 && tileAttachEnabled) {
        tileSessions.resize(numTiles);

        for (uint32_t i = 0; i < numTiles; i++) {
            auto subDevice = connectedDevice->getNEODevice()->getSubDevice(i)->getSpecializedDevice<Device>();
            tileSessions[i] = std::pair<DebugSessionImp *, bool>{createTileSession(config, subDevice, this), false};
        }
        tileSessionsEnabled = true;
    }
}

TileDebugSessionLinux *DebugSessionLinux::createTileSession(const zet_debug_config_t &config, Device *device, DebugSessionImp *rootDebugSession) {
    auto tileSession = new TileDebugSessionLinux(config, device, rootDebugSession);
    tileSession->initialize();
    return tileSession;
}

void *DebugSessionLinux::asyncThreadFunction(void *arg) {
    DebugSessionLinux *self = reinterpret_cast<DebugSessionLinux *>(arg);
    PRINT_DEBUGGER_INFO_LOG("Debugger async thread start\n", "");

    while (self->asyncThread.threadActive) {
        self->handleEventsAsync();

        if (self->tileSessionsEnabled) {
            for (size_t tileIndex = 0; tileIndex < self->tileSessions.size(); tileIndex++) {
                static_cast<TileDebugSessionLinux *>(self->tileSessions[tileIndex].first)->sendInterrupts();
                static_cast<TileDebugSessionLinux *>(self->tileSessions[tileIndex].first)->generateEventsAndResumeStoppedThreads();
            }
        } else {
            self->sendInterrupts();
            self->generateEventsAndResumeStoppedThreads();
        }
    }

    PRINT_DEBUGGER_INFO_LOG("Debugger async thread closing\n", "");

    return nullptr;
}

void *DebugSessionLinux::readInternalEventsThreadFunction(void *arg) {
    DebugSessionLinux *self = reinterpret_cast<DebugSessionLinux *>(arg);
    PRINT_DEBUGGER_INFO_LOG("Debugger internal event thread started\n", "");
    self->internalThreadHasStarted = true;

    while (self->internalEventThread.threadActive) {
        self->readInternalEventsAsync();
    }

    PRINT_DEBUGGER_INFO_LOG("Debugger internal event thread closing\n", "");

    return nullptr;
}

void DebugSessionLinux::startAsyncThread() {
    asyncThread.thread = NEO::Thread::create(asyncThreadFunction, reinterpret_cast<void *>(this));
}

void DebugSessionLinux::closeAsyncThread() {
    asyncThread.close();
    internalEventThread.close();
}

bool DebugSessionLinux::closeFd() {
    if (fd == 0) {
        return false;
    }

    auto ret = NEO::SysCalls::close(fd);

    if (ret != 0) {
        PRINT_DEBUGGER_ERROR_LOG("Debug connection close() on fd: %d failed: retCode: %d\n", fd, ret);
        return false;
    }
    fd = 0;
    return true;
}

std::unique_ptr<uint64_t[]> DebugSessionLinux::getInternalEvent() {
    std::unique_ptr<uint64_t[]> eventMemory;

    {
        std::unique_lock<std::mutex> lock(internalEventThreadMutex);

        if (internalEventQueue.empty()) {
            internalEventCondition.wait_for(lock, std::chrono::milliseconds(100));
        }

        if (!internalEventQueue.empty()) {
            eventMemory = std::move(internalEventQueue.front());
            internalEventQueue.pop();
        }
    }
    return eventMemory;
}

void DebugSessionLinux::handleEventsAsync() {
    auto eventMemory = getInternalEvent();
    if (eventMemory != nullptr) {
        auto debugEvent = reinterpret_cast<prelim_drm_i915_debug_event *>(eventMemory.get());
        handleEvent(debugEvent);

        if (debugEvent->type != PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND && pendingVmBindEvents.size() > 0) {
            processPendingVmBindEvents();
        }
    }
}

void DebugSessionLinux::readInternalEventsAsync() {

    struct pollfd pollFd = {
        .fd = fd,
        .events = POLLIN,
        .revents = 0,
    };

    int pollTimeout = 1000;
    auto numberOfFds = ioctlHandler->poll(&pollFd, 1, pollTimeout);

    PRINT_DEBUGGER_INFO_LOG("Debugger async thread readEvent poll() retCode: %d\n", numberOfFds);

    if (!detached && numberOfFds < 0 && errno == EINVAL) {
        zet_debug_event_t debugEvent = {};
        debugEvent.type = ZET_DEBUG_EVENT_TYPE_DETACHED;
        debugEvent.info.detached.reason = ZET_DEBUG_DETACH_REASON_INVALID;

        PRINT_DEBUGGER_INFO_LOG("Debugger detached\n", "");

        if (tileSessionsEnabled) {
            auto numTiles = connectedDevice->getNEODevice()->getNumSubDevices();
            for (uint32_t tileIndex = 0; tileIndex < numTiles; tileIndex++) {
                static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->pushApiEvent(debugEvent);
                static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->detached = true;
            }
        } else {
            pushApiEvent(debugEvent);
        }
        detached = true;
    } else if (numberOfFds > 0) {

        ze_result_t result = ZE_RESULT_SUCCESS;

        int maxLoopCount = 3;
        do {

            uint8_t maxEventBuffer[sizeof(prelim_drm_i915_debug_event) + maxEventSize];
            auto event = reinterpret_cast<prelim_drm_i915_debug_event *>(maxEventBuffer);
            event->size = maxEventSize;
            event->type = PRELIM_DRM_I915_DEBUG_EVENT_READ;
            event->flags = 0;

            result = readEventImp(event);
            maxLoopCount--;

            if (result == ZE_RESULT_SUCCESS) {
                std::lock_guard<std::mutex> lock(internalEventThreadMutex);

                auto memory = std::make_unique<uint64_t[]>(maxEventSize / sizeof(uint64_t));
                memcpy(memory.get(), event, maxEventSize);

                internalEventQueue.push(std::move(memory));
                internalEventCondition.notify_one();
            }
        } while (result == ZE_RESULT_SUCCESS && maxLoopCount > 0);
    }
}

bool DebugSessionLinux::closeConnection() {
    closeAsyncThread();
    closeInternalEventsThread();
    return closeFd();
}

void DebugSessionLinux::handleEvent(prelim_drm_i915_debug_event *event) {
    auto type = event->type;

    PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type = %lu flags = %d seqno = %llu size = %llu",
                            (uint32_t)event->type, (int)event->flags, (uint64_t)event->seqno, (uint64_t)event->size);

    switch (type) {
    case PRELIM_DRM_I915_DEBUG_EVENT_CLIENT: {
        auto clientEvent = reinterpret_cast<prelim_drm_i915_debug_event_client *>(event);

        if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE) {
            DEBUG_BREAK_IF(clientHandleToConnection.find(clientEvent->handle) != clientHandleToConnection.end());
            clientHandleToConnection[clientEvent->handle].reset(new ClientConnection);
            clientHandleToConnection[clientEvent->handle]->client = *clientEvent;
        }

        if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY) {
            clientHandleClosed = clientEvent->handle;
        }

        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_CLIENT flags = %d size = %llu client.handle = %llu\n",
                                (int)event->flags, (uint64_t)event->size, (uint64_t)clientEvent->handle);

    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT: {
        prelim_drm_i915_debug_event_context *context = reinterpret_cast<prelim_drm_i915_debug_event_context *>(event);

        if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE) {
            UNRECOVERABLE_IF(clientHandleToConnection.find(context->client_handle) == clientHandleToConnection.end());
            clientHandleToConnection[context->client_handle]->contextsCreated[context->handle].handle = context->handle;
        }

        if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY) {
            clientHandleToConnection[context->client_handle]->contextsCreated.erase(context->handle);
        }

        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT flags = %d size = %llu client_handle = %llu handle = %llu\n",
                                (int)event->flags, (uint64_t)event->size, (uint64_t)context->client_handle, (uint64_t)context->handle);
    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_UUID: {
        prelim_drm_i915_debug_event_uuid *uuid = reinterpret_cast<prelim_drm_i915_debug_event_uuid *>(event);

        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_UUID flags = %d size = %llu client_handle = %llu handle = %llu class_handle = %llu payload_size = %llu\n",
                                (int)event->flags, (uint64_t)event->size, (uint64_t)uuid->client_handle, (uint64_t)uuid->handle, (uint64_t)uuid->class_handle, (uint64_t)uuid->payload_size);

        bool destroy = event->flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
        bool create = event->flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE;

        if (destroy && clientHandleToConnection[uuid->client_handle]->uuidMap[uuid->handle].classIndex == NEO::DrmResourceClass::L0ZebinModule) {
            DEBUG_BREAK_IF(clientHandleToConnection[uuid->client_handle]->uuidToModule[uuid->handle].segmentVmBindCounter[0] != 0 ||
                           clientHandleToConnection[uuid->client_handle]->uuidToModule[uuid->handle].loadAddresses[0].size() > 0);

            clientHandleToConnection[uuid->client_handle]->uuidToModule.erase(uuid->handle);
        }

        if (destroy && (clientHandle == uuid->client_handle)) {

            for (const auto &uuidToDevice : uuidL0CommandQueueHandleToDevice) {
                if (uuidToDevice.first == uuid->handle) {
                    auto deviceIndex = uuidToDevice.second;
                    uuidL0CommandQueueHandleToDevice.erase(uuidToDevice.first);

                    zet_debug_event_t debugEvent = {};
                    debugEvent.type = ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT;

                    if (tileSessionsEnabled) {
                        auto tileSession = reinterpret_cast<TileDebugSessionLinux *>(tileSessions[deviceIndex].first);
                        tileSession->processExit();
                        tileSession->pushApiEvent(debugEvent);
                    } else if (uuidL0CommandQueueHandleToDevice.size() == 0) {
                        pushApiEvent(debugEvent);
                    }

                    break;
                }
            }
            break;
        }

        if (create) {
            const auto &connection = clientHandleToConnection[uuid->client_handle];
            if (uuid->payload_size) {
                prelim_drm_i915_debug_read_uuid readUuid = {};
                auto payload = std::make_unique<char[]>(uuid->payload_size);
                readUuid.client_handle = uuid->client_handle;
                readUuid.handle = static_cast<decltype(readUuid.handle)>(uuid->handle);
                readUuid.payload_ptr = reinterpret_cast<uint64_t>(payload.get());
                readUuid.payload_size = uuid->payload_size;
                auto ret = ioctl(PRELIM_I915_DEBUG_IOCTL_READ_UUID, &readUuid);

                if (ret == 0) {
                    std::string uuidString = std::string(readUuid.uuid, 36);
                    uint32_t classIndex = static_cast<uint32_t>(NEO::DrmResourceClass::MaxSize);
                    auto validClassUuid = NEO::DrmUuid::getClassUuidIndex(uuidString, classIndex);

                    if (uuidString == NEO::uuidL0CommandQueueHash) {
                        if ((clientHandle == invalidClientHandle) || (clientHandle == uuid->client_handle)) {
                            clientHandle = uuid->client_handle;

                            uint32_t deviceIndex = 0;
                            if (readUuid.payload_size == sizeof(NEO::DebuggerL0::CommandQueueNotification)) {
                                auto notification = reinterpret_cast<NEO::DebuggerL0::CommandQueueNotification *>(payload.get());
                                deviceIndex = notification->subDeviceIndex;
                                UNRECOVERABLE_IF(notification->subDeviceCount > 0 && notification->subDeviceIndex >= notification->subDeviceCount);
                            }

                            zet_debug_event_t debugEvent = {};
                            debugEvent.type = ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY;

                            if (tileSessionsEnabled) {
                                UNRECOVERABLE_IF(uuidL0CommandQueueHandleToDevice.find(uuid->handle) != uuidL0CommandQueueHandleToDevice.end());
                                auto tileSession = static_cast<TileDebugSessionLinux *>(tileSessions[deviceIndex].first);
                                tileSession->processEntry();
                                tileSession->pushApiEvent(debugEvent);
                            } else if (uuidL0CommandQueueHandleToDevice.size() == 0) {
                                pushApiEvent(debugEvent);
                            }
                            uuidL0CommandQueueHandleToDevice[uuid->handle] = deviceIndex;
                        }
                    }

                    if (validClassUuid) {
                        if (clientHandle == invalidClientHandle) {
                            clientHandle = uuid->client_handle;
                        }
                        std::string className(reinterpret_cast<char *>(readUuid.payload_ptr), readUuid.payload_size);
                        connection->classHandleToIndex[uuid->handle] = {className, classIndex};
                    } else {
                        auto &uuidData = connection->uuidMap[uuid->handle];
                        uuidData.classHandle = uuid->class_handle;
                        uuidData.handle = uuid->handle;
                        uuidData.data = std::move(payload);
                        uuidData.dataSize = uuid->payload_size;
                        uuidData.classIndex = NEO::DrmResourceClass::MaxSize;

                        const auto indexIt = connection->classHandleToIndex.find(uuid->class_handle);
                        if (indexIt != connection->classHandleToIndex.end()) {
                            uuidData.classIndex = static_cast<NEO::DrmResourceClass>(indexIt->second.second);
                        }

                        if (uuidData.classIndex == NEO::DrmResourceClass::Elf) {
                            auto cpuVa = extractVaFromUuidString(uuidString);
                            uuidData.ptr = cpuVa;
                        }

                        if (uuidData.classIndex == NEO::DrmResourceClass::L0ZebinModule) {
                            uint64_t handle = uuid->handle;

                            auto &newModule = connection->uuidToModule[handle];
                            newModule.segmentCount = 0;
                            newModule.moduleUuidHandle = handle;
                            for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
                                newModule.segmentVmBindCounter[i] = 0;
                                newModule.loadAddresses[i].clear();
                                newModule.moduleLoadEventAcked[i] = false;
                            }
                        }
                        extractUuidData(uuid->client_handle, uuidData);
                    }

                    PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_UUID client_handle = %llu handle = %llu flags = %d uuid = %s ret = %d\n",
                                            (uint64_t)readUuid.client_handle, (uint64_t)readUuid.handle, (int)readUuid.flags, uuidString.c_str(), ret);
                } else {
                    PRINT_DEBUGGER_ERROR_LOG("PRELIM_I915_DEBUG_IOCTL_READ_UUID ret = %d errno = %d\n", ret, errno);
                }
            } else {
                connection->uuidMap[uuid->handle].classHandle = uuid->class_handle;
                connection->uuidMap[uuid->handle].handle = uuid->handle;
            }
        }

    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_VM: {
        prelim_drm_i915_debug_event_vm *vm = reinterpret_cast<prelim_drm_i915_debug_event_vm *>(event);

        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_VM flags = %d size = %llu client_handle = %llu handle = %llu\n",
                                (int)event->flags, (uint64_t)event->size, (uint64_t)vm->client_handle, (uint64_t)vm->handle);

        if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE) {
            UNRECOVERABLE_IF(clientHandleToConnection.find(vm->client_handle) == clientHandleToConnection.end());
            clientHandleToConnection[vm->client_handle]->vmIds.emplace(static_cast<uint64_t>(vm->handle));
        }

        if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY) {
            UNRECOVERABLE_IF(clientHandleToConnection.find(vm->client_handle) == clientHandleToConnection.end());
            clientHandleToConnection[vm->client_handle]->vmIds.erase(static_cast<uint64_t>(vm->handle));
        }
    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND: {
        prelim_drm_i915_debug_event_vm_bind *vmBind = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(event);

        if (!handleVmBindEvent(vmBind)) {
            if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK) {
                prelim_drm_i915_debug_event_ack eventToAck = {};
                eventToAck.type = vmBind->base.type;
                eventToAck.seqno = vmBind->base.seqno;
                eventToAck.flags = 0;
                auto ret = ioctl(PRELIM_I915_DEBUG_IOCTL_ACK_EVENT, &eventToAck);
                PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_ACK_EVENT seqno = %llu ret = %d errno = %d\n", (uint64_t)eventToAck.seqno, ret, ret != 0 ? errno : 0);
            } else {
                auto sizeAligned = alignUp(event->size, sizeof(uint64_t));
                auto pendingEvent = std::make_unique<uint64_t[]>(sizeAligned / sizeof(uint64_t));
                memcpy_s(pendingEvent.get(), sizeAligned, event, event->size);
                pendingVmBindEvents.push_back(std::move(pendingEvent));
            }
        }

    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT_PARAM: {
        prelim_drm_i915_debug_event_context_param *contextParam = reinterpret_cast<prelim_drm_i915_debug_event_context_param *>(event);
        handleContextParamEvent(contextParam);
    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION: {
        prelim_drm_i915_debug_event_eu_attention *attention = reinterpret_cast<prelim_drm_i915_debug_event_eu_attention *>(event);

        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION flags = %d, seqno = %d, size = %llu"
                                " client_handle = %llu flags = %llu class = %lu instance = %lu bitmask_size = %lu ctx_handle = %llu\n",
                                (int)attention->base.flags, (uint64_t)attention->base.seqno, (uint64_t)attention->base.size,
                                (uint64_t)attention->client_handle, (uint64_t)attention->flags, (uint32_t)attention->ci.engine_class,
                                (uint32_t)attention->ci.engine_instance, (uint32_t)attention->bitmask_size, uint64_t(attention->ctx_handle));

        handleAttentionEvent(attention);
    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_ENGINES: {
        prelim_drm_i915_debug_event_engines *engines = reinterpret_cast<prelim_drm_i915_debug_event_engines *>(event);
        handleEnginesEvent(engines);
    } break;
    default:
        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: UNHANDLED %d flags = %d size = %llu\n", (int)event->type, (int)event->flags, (uint64_t)event->size);
        break;
    }
}

void DebugSessionLinux::processPendingVmBindEvents() {
    size_t processedEvents = 0;
    for (size_t index = 0; index < pendingVmBindEvents.size(); index++) {
        auto debugEvent = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(pendingVmBindEvents[index].get());
        if (handleVmBindEvent(debugEvent) == false) {
            break;
        }
        processedEvents++;
    }
    if (processedEvents > 0) {
        pendingVmBindEvents.erase(pendingVmBindEvents.begin(), pendingVmBindEvents.begin() + processedEvents);
    }
}

bool DebugSessionLinux::checkAllEventsCollected() {
    bool allEventsCollected = false;
    bool clientConnected = (this->clientHandle != invalidClientHandle);
    if (clientConnected) {
        if (clientHandleToConnection[clientHandle]->vmToModuleDebugAreaBindInfo.size() > 0) {
            allEventsCollected = true;
        }
    }
    return allEventsCollected;
}

bool DebugSessionLinux::readModuleDebugArea() {
    auto vm = clientHandleToConnection[clientHandle]->vmToModuleDebugAreaBindInfo.begin()->first;
    auto gpuVa = clientHandleToConnection[clientHandle]->vmToModuleDebugAreaBindInfo.begin()->second.gpuVa;

    memset(this->debugArea.magic, 0, sizeof(this->debugArea.magic));
    auto retVal = readGpuMemory(vm, reinterpret_cast<char *>(&this->debugArea), sizeof(this->debugArea), gpuVa);

    if (retVal != ZE_RESULT_SUCCESS || strncmp(this->debugArea.magic, "dbgarea", sizeof(NEO::DebugAreaHeader::magic)) != 0) {
        PRINT_DEBUGGER_ERROR_LOG("Reading Module Debug Area failed, error = %d\n", retVal);
        return false;
    }

    return true;
}

void DebugSessionLinux::readStateSaveAreaHeader() {
    if (clientHandle == invalidClientHandle) {
        return;
    }

    uint64_t vm = 0;
    uint64_t gpuVa = 0;
    size_t totalSize = 0;

    {
        std::lock_guard<std::mutex> lock(asyncThreadMutex);
        if (clientHandleToConnection[clientHandle]->vmToContextStateSaveAreaBindInfo.size() > 0) {
            vm = clientHandleToConnection[clientHandle]->vmToContextStateSaveAreaBindInfo.begin()->first;
            gpuVa = clientHandleToConnection[clientHandle]->vmToContextStateSaveAreaBindInfo.begin()->second.gpuVa;
            totalSize = clientHandleToConnection[clientHandle]->vmToContextStateSaveAreaBindInfo.begin()->second.size;
        }
    }

    if (gpuVa > 0) {
        auto headerSize = sizeof(SIP::StateSaveAreaHeader);

        if (totalSize < headerSize) {
            PRINT_DEBUGGER_ERROR_LOG("Context State Save Area size incorrect\n", "");
            return;
        } else {
            validateAndSetStateSaveAreaHeader(vm, gpuVa);
        }
    }
}

ze_result_t DebugSessionLinux::readEventImp(prelim_drm_i915_debug_event *drmDebugEvent) {
    auto ret = ioctl(PRELIM_I915_DEBUG_IOCTL_READ_EVENT, drmDebugEvent);

    if (ret != 0) {
        PRINT_DEBUGGER_ERROR_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT failed: retCode: %d errno = %d\n", ret, errno);
    } else {
        if ((drmDebugEvent->flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE) == 0 &&
            (drmDebugEvent->flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY) == 0 &&
            (drmDebugEvent->flags & PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE) == 0) {

            PRINT_DEBUGGER_ERROR_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT unsupported flag = %d\n", (int)drmDebugEvent->flags);
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_NOT_READY;
}

bool DebugSessionLinux::handleVmBindEvent(prelim_drm_i915_debug_event_vm_bind *vmBind) {

    PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND flags = %d size = %llu client_handle = %llu vm_handle = %llu va_start = %p va_lenght = %llu num_uuids = %lu\n",
                            (int)vmBind->base.flags, (uint64_t)vmBind->base.size, (uint64_t)vmBind->client_handle, (uint64_t)vmBind->vm_handle, (void *)vmBind->va_start, (uint64_t)vmBind->va_length, (uint32_t)vmBind->num_uuids);

    const bool createEvent = (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE);
    const bool destroyEvent = (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY);

    bool shouldAckEvent = true;

    if (vmBind->num_uuids > 0 && vmBind->base.size > sizeof(prelim_drm_i915_debug_event_vm_bind)) {
        auto vmHandle = vmBind->vm_handle;
        auto connection = clientHandleToConnection[vmBind->client_handle].get();
        uint32_t index = 0;
        const auto uuid = vmBind->uuids[index];

        if (connection->uuidMap.find(uuid) == connection->uuidMap.end()) {
            PRINT_DEBUGGER_ERROR_LOG("Unknown UUID handle = %llu\n", (uint64_t)uuid);
            return false;
        }

        if (connection->vmToTile.find(vmHandle) == connection->vmToTile.end()) {
            DEBUG_BREAK_IF(connection->vmToTile.find(vmHandle) == connection->vmToTile.end() && (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK) &&
                           (connection->uuidMap[uuid].classIndex == NEO::DrmResourceClass::Isa || connection->uuidMap[uuid].classIndex == NEO::DrmResourceClass::ModuleHeapDebugArea));
            return false;
        }

        const auto tileIndex = connection->vmToTile[vmHandle];

        PRINT_DEBUGGER_INFO_LOG("UUID handle = %llu class index = %d\n", static_cast<uint64_t>(vmBind->uuids[index]), static_cast<int>(clientHandleToConnection[vmBind->client_handle]->uuidMap[vmBind->uuids[index]].classIndex));

        auto classUuid = connection->uuidMap[uuid].classHandle;

        if (connection->classHandleToIndex.find(classUuid) != connection->classHandleToIndex.end()) {

            std::lock_guard<std::mutex> lock(asyncThreadMutex);

            if (connection->classHandleToIndex[classUuid].second ==
                static_cast<uint32_t>(NEO::DrmResourceClass::SbaTrackingBuffer)) {
                connection->vmToStateBaseAreaBindInfo[vmHandle] = {vmBind->va_start, vmBind->va_length};
            }

            if (connection->classHandleToIndex[classUuid].second ==
                static_cast<uint32_t>(NEO::DrmResourceClass::ModuleHeapDebugArea)) {
                connection->vmToModuleDebugAreaBindInfo[vmHandle] = {vmBind->va_start, vmBind->va_length};
            }

            if (connection->classHandleToIndex[classUuid].second ==
                static_cast<uint32_t>(NEO::DrmResourceClass::ContextSaveArea)) {
                connection->vmToContextStateSaveAreaBindInfo[vmHandle] = {vmBind->va_start, vmBind->va_length};
            }
        }

        bool handleEvent = isTileWithinDeviceBitfield(tileIndex);

        if (handleEvent && connection->uuidMap[uuid].classIndex == NEO::DrmResourceClass::Isa) {

            uint32_t deviceBitfield = 0;
            memcpy_s(&deviceBitfield, sizeof(uint32_t), connection->uuidMap[uuid].data.get(), connection->uuidMap[uuid].dataSize);
            const NEO::DeviceBitfield devices(deviceBitfield);

            PRINT_DEBUGGER_INFO_LOG("ISA vm_handle = %llu, tileIndex = %lu, deviceBitfield = %llu", vmHandle, tileIndex, devices.to_ulong());

            const auto isaUuidHandle = connection->uuidMap[uuid].handle;
            bool perKernelModules = true;
            int moduleUUIDindex = -1;
            bool tileInstanced = false;
            bool allInstancesEventsReceived = true;

            for (uint32_t uuidIter = 1; uuidIter < vmBind->num_uuids; uuidIter++) {
                if (connection->uuidMap[vmBind->uuids[uuidIter]].classIndex == NEO::DrmResourceClass::L0ZebinModule) {
                    perKernelModules = false;
                    moduleUUIDindex = static_cast<int>(uuidIter);
                }

                if (connection->uuidMap[vmBind->uuids[uuidIter]].classHandle == isaUuidHandle) {
                    tileInstanced = true;
                }
            }

            if (connection->isaMap[tileIndex].find(vmBind->va_start) == connection->isaMap[tileIndex].end() && createEvent) {

                auto &isaMap = connection->isaMap[tileIndex];
                auto &elfMap = connection->elfMap;

                auto isa = std::make_unique<IsaAllocation>();
                isa->bindInfo = {vmBind->va_start, vmBind->va_length};
                isa->vmHandle = vmHandle;
                isa->elfUuidHandle = invalidHandle;
                isa->moduleBegin = 0;
                isa->moduleEnd = 0;
                isa->tileInstanced = tileInstanced;
                isa->perKernelModule = perKernelModules;
                isa->deviceBitfield = devices;

                for (index = 1; index < vmBind->num_uuids; index++) {
                    if (connection->uuidMap[vmBind->uuids[index]].classIndex == NEO::DrmResourceClass::Elf) {
                        isa->elfUuidHandle = vmBind->uuids[index];

                        if (!perKernelModules) {
                            auto &module = connection->uuidToModule[vmBind->uuids[moduleUUIDindex]];
                            DEBUG_BREAK_IF(module.elfUuidHandle != 0 && connection->uuidMap[vmBind->uuids[index]].ptr != connection->uuidMap[module.elfUuidHandle].ptr);

                            module.elfUuidHandle = vmBind->uuids[index];
                            module.deviceBitfield = devices;
                        }
                    }
                }

                if (isa->elfUuidHandle != invalidHandle) {
                    isa->moduleBegin = connection->uuidMap[isa->elfUuidHandle].ptr;
                    isa->moduleEnd = isa->moduleBegin + connection->uuidMap[isa->elfUuidHandle].dataSize;
                    elfMap[isa->moduleBegin] = isa->elfUuidHandle;
                } else {
                    PRINT_DEBUGGER_ERROR_LOG("No ELF provided by application\n", "");
                }

                auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
                auto loadAddress = gmmHelper->canonize(vmBind->va_start);

                zet_debug_event_t debugEvent = {};
                debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_LOAD;
                debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
                debugEvent.info.module.load = loadAddress;
                debugEvent.info.module.moduleBegin = isa->moduleBegin;
                debugEvent.info.module.moduleEnd = isa->moduleEnd;

                std::unique_lock<std::mutex> memLock(asyncThreadMutex);
                isaMap[vmBind->va_start] = std::move(isa);

                // Expect non canonical va_start
                DEBUG_BREAK_IF(gmmHelper->decanonize(vmBind->va_start) != vmBind->va_start);

                bool apiEventNeedsAck = (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK);
                // If ACK flag is not set when triggering MODULE LOAD event, auto-ack immediately
                if (apiEventNeedsAck == false) {
                    isaMap[vmBind->va_start]->moduleLoadEventAck = true;
                }

                if (tileSessionsEnabled) {
                    auto tileAttached = tileSessions[tileIndex].second;

                    if (!tileAttached) {
                        isaMap[vmBind->va_start]->moduleLoadEventAck = true;
                        apiEventNeedsAck = false;
                    }

                    PRINT_DEBUGGER_INFO_LOG("TileDebugSession attached = %d, tileIndex = %lu, apiEventNeedsAck = %d", (int)tileAttached, tileIndex, (int)apiEventNeedsAck);
                }
                memLock.unlock();

                if (perKernelModules) {
                    debugEvent.flags = apiEventNeedsAck ? ZET_DEBUG_EVENT_FLAG_NEED_ACK : 0;

                    if (tileSessionsEnabled) {
                        auto tileAttached = static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->insertModule(debugEvent.info.module);
                        if (tileAttached) {
                            static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->pushApiEvent(debugEvent);
                        }
                    } else {

                        if (devices.count() > 1) {
                            allInstancesEventsReceived = checkAllOtherTileIsaAllocationsPresent(tileIndex, vmBind->va_start);
                        }
                        if (allInstancesEventsReceived) {
                            pushApiEvent(debugEvent);
                        }
                    }
                }
            }

            if (createEvent) {
                std::lock_guard<std::mutex> lock(asyncThreadMutex);

                if (!connection->isaMap[tileIndex][vmBind->va_start]->moduleLoadEventAck && perKernelModules) {
                    bool doNotAutoAckEvent = (!blockOnFenceMode && allInstancesEventsReceived); // in block on CPU mode - do not auto-ack last event for isa instance
                    doNotAutoAckEvent |= blockOnFenceMode;                                      // in block on fence mode - do not auto-ack any events

                    if (doNotAutoAckEvent) {
                        PRINT_DEBUGGER_INFO_LOG("Add event to ack, seqno = %llu", (uint64_t)vmBind->base.seqno);
                        connection->isaMap[tileIndex][vmBind->va_start]->ackEvents.push_back(vmBind->base);
                        shouldAckEvent = false;
                    }
                }

                connection->isaMap[tileIndex][vmBind->va_start]->vmBindCounter++;
            }

            if (destroyEvent && connection->isaMap[tileIndex].find(vmBind->va_start) != connection->isaMap[tileIndex].end()) {
                DEBUG_BREAK_IF(connection->isaMap[tileIndex][vmBind->va_start]->vmBindCounter == 0);
                connection->isaMap[tileIndex][vmBind->va_start]->vmBindCounter--;
                if (connection->isaMap[tileIndex][vmBind->va_start]->vmBindCounter == 0) {
                    const auto &isa = connection->isaMap[tileIndex][vmBind->va_start];

                    zet_debug_event_t debugEvent = {};

                    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
                    auto loadAddress = gmmHelper->canonize(isa->bindInfo.gpuVa);

                    debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD;
                    debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
                    debugEvent.info.module.load = loadAddress;
                    debugEvent.info.module.moduleBegin = isa->moduleBegin;
                    debugEvent.info.module.moduleEnd = isa->moduleEnd;

                    if (perKernelModules) {
                        if (tileSessionsEnabled) {
                            static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->removeModule(debugEvent.info.module);
                            static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->pushApiEvent(debugEvent);
                        } else {
                            bool notifyEvent = true;
                            if (isa->deviceBitfield.count() > 1) {
                                notifyEvent = checkAllOtherTileIsaAllocationsRemoved(tileIndex, vmBind->va_start);
                            }
                            if (notifyEvent) {
                                pushApiEvent(debugEvent);
                            }
                        }
                    }
                    std::unique_lock<std::mutex> memLock(asyncThreadMutex);
                    connection->isaMap[tileIndex].erase(vmBind->va_start);
                    memLock.unlock();
                }
            }
        }

        if (handleEvent) {

            for (uint32_t uuidIter = 0; uuidIter < vmBind->num_uuids; uuidIter++) {
                if (connection->uuidMap[vmBind->uuids[uuidIter]].classIndex == NEO::DrmResourceClass::L0ZebinModule) {
                    uint64_t loadAddress = 0;
                    auto &module = connection->uuidToModule[vmBind->uuids[uuidIter]];

                    if (createEvent) {
                        module.segmentVmBindCounter[tileIndex]++;

                        DEBUG_BREAK_IF(module.loadAddresses[tileIndex].size() > module.segmentCount);
                        bool canTriggerEvent = module.loadAddresses[tileIndex].size() == (module.segmentCount - 1);
                        module.loadAddresses[tileIndex].insert(vmBind->va_start);

                        if (!blockOnFenceMode) {
                            if (canTriggerEvent && module.loadAddresses[tileIndex].size() == module.segmentCount) {
                                auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
                                loadAddress = gmmHelper->canonize(*std::min_element(module.loadAddresses[tileIndex].begin(), module.loadAddresses[tileIndex].end()));
                                PRINT_DEBUGGER_INFO_LOG("Zebin module loaded at: %p, with %u isa allocations", (void *)loadAddress, module.segmentCount);

                                zet_debug_event_t debugEvent = {};
                                debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_LOAD;
                                debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
                                debugEvent.info.module.load = loadAddress;
                                debugEvent.info.module.moduleBegin = connection->uuidMap[module.elfUuidHandle].ptr;
                                debugEvent.info.module.moduleEnd = connection->uuidMap[module.elfUuidHandle].ptr + connection->uuidMap[module.elfUuidHandle].dataSize;

                                if (!tileSessionsEnabled) {
                                    bool allInstancesEventsReceived = true;
                                    if (module.deviceBitfield.count() > 1) {
                                        allInstancesEventsReceived = checkAllOtherTileModuleSegmentsPresent(tileIndex, module);
                                    }
                                    if (allInstancesEventsReceived) {
                                        if (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK) {
                                            debugEvent.flags = ZET_DEBUG_EVENT_FLAG_NEED_ACK;
                                            module.ackEvents[tileIndex].push_back(vmBind->base);
                                        }
                                        pushApiEvent(debugEvent, vmBind->uuids[uuidIter]);
                                        shouldAckEvent = false;
                                    }
                                } else {
                                    auto tileAttached = static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->insertModule(debugEvent.info.module);

                                    if (tileAttached) {
                                        if (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK) {
                                            debugEvent.flags = ZET_DEBUG_EVENT_FLAG_NEED_ACK;
                                            module.ackEvents[tileIndex].push_back(vmBind->base);
                                        }
                                        static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->pushApiEvent(debugEvent, vmBind->uuids[uuidIter]);
                                        shouldAckEvent = false;
                                    }
                                }
                            }
                        } else {
                            if (canTriggerEvent && module.loadAddresses[tileIndex].size() == module.segmentCount) {
                                auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
                                loadAddress = gmmHelper->canonize(*std::min_element(module.loadAddresses[tileIndex].begin(), module.loadAddresses[tileIndex].end()));
                                PRINT_DEBUGGER_INFO_LOG("Zebin module loaded at: %p, with %u isa allocations", (void *)loadAddress, module.segmentCount);

                                zet_debug_event_t debugEvent = {};
                                debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_LOAD;
                                debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
                                debugEvent.info.module.load = loadAddress;
                                debugEvent.info.module.moduleBegin = connection->uuidMap[module.elfUuidHandle].ptr;
                                debugEvent.info.module.moduleEnd = connection->uuidMap[module.elfUuidHandle].ptr + connection->uuidMap[module.elfUuidHandle].dataSize;
                                if (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK) {
                                    debugEvent.flags = ZET_DEBUG_EVENT_FLAG_NEED_ACK;
                                }

                                if (!tileSessionsEnabled) {
                                    bool allInstancesEventsReceived = true;
                                    if (module.deviceBitfield.count() > 1) {
                                        allInstancesEventsReceived = checkAllOtherTileModuleSegmentsPresent(tileIndex, module);
                                    }
                                    if (allInstancesEventsReceived) {
                                        pushApiEvent(debugEvent, vmBind->uuids[uuidIter]);
                                        shouldAckEvent = false;
                                    }
                                } else {
                                    auto tileAttached = static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->insertModule(debugEvent.info.module);
                                    if (tileAttached) {
                                        static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->pushApiEvent(debugEvent, vmBind->uuids[uuidIter]);
                                        shouldAckEvent = false;
                                    }
                                }
                            }
                            {
                                std::lock_guard<std::mutex> lock(asyncThreadMutex);
                                if (!module.moduleLoadEventAcked[tileIndex]) {
                                    shouldAckEvent = false;
                                }

                                if (tileSessionsEnabled && !static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->isAttached) {
                                    shouldAckEvent = true;
                                }
                                if (!shouldAckEvent && (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK)) {
                                    module.ackEvents[tileIndex].push_back(vmBind->base);
                                }
                            }
                        }

                    } else { // destroyEvent

                        module.segmentVmBindCounter[tileIndex]--;

                        if (module.segmentVmBindCounter[tileIndex] == 0) {

                            zet_debug_event_t debugEvent = {};

                            auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
                            auto loadAddress = gmmHelper->canonize(*std::min_element(module.loadAddresses[tileIndex].begin(), module.loadAddresses[tileIndex].end()));
                            debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD;
                            debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
                            debugEvent.info.module.load = loadAddress;
                            debugEvent.info.module.moduleBegin = connection->uuidMap[module.elfUuidHandle].ptr;
                            debugEvent.info.module.moduleEnd = connection->uuidMap[module.elfUuidHandle].ptr + connection->uuidMap[module.elfUuidHandle].dataSize;

                            if (tileSessionsEnabled) {

                                auto tileAttached = static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->removeModule(debugEvent.info.module);

                                if (tileAttached) {
                                    static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->pushApiEvent(debugEvent, vmBind->uuids[uuidIter]);
                                }

                            } else {
                                bool notifyEvent = true;
                                if (module.deviceBitfield.count() > 1) {
                                    notifyEvent = checkAllOtherTileModuleSegmentsRemoved(tileIndex, module);
                                }
                                if (notifyEvent) {
                                    pushApiEvent(debugEvent, vmBind->uuids[uuidIter]);
                                }
                            }
                            module.loadAddresses[tileIndex].clear();
                            module.moduleLoadEventAcked[tileIndex] = false;
                        }
                    }
                    break;
                }
            }
        }
    }

    if (shouldAckEvent && (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK)) {
        prelim_drm_i915_debug_event_ack eventToAck = {};
        eventToAck.type = vmBind->base.type;
        eventToAck.seqno = vmBind->base.seqno;
        eventToAck.flags = 0;
        auto ret = ioctl(PRELIM_I915_DEBUG_IOCTL_ACK_EVENT, &eventToAck);
        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_ACK_EVENT seqno = %llu ret = %d errno = %d\n", (uint64_t)eventToAck.seqno, ret, ret != 0 ? errno : 0);
    }
    return true;
}

void DebugSessionLinux::handleContextParamEvent(prelim_drm_i915_debug_event_context_param *contextParam) {

    PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT_PARAM flags = %d size = %llu client_handle = %llu ctx_handle = %llu\n",
                            (int)contextParam->base.flags, (uint64_t)contextParam->base.size, (uint64_t)contextParam->client_handle, (uint64_t)contextParam->ctx_handle);

    if (clientHandleToConnection[contextParam->client_handle]->contextsCreated.find(contextParam->ctx_handle) ==
        clientHandleToConnection[contextParam->client_handle]->contextsCreated.end()) {
        PRINT_DEBUGGER_ERROR_LOG("CONTEXT handle does not exist\n", "");
        return;
    }

    switch (contextParam->param.param) {
    case I915_CONTEXT_PARAM_VM:
        PRINT_DEBUGGER_INFO_LOG("I915_CONTEXT_PARAM_VM vm = %llu\n", (uint64_t)contextParam->param.value);
        clientHandleToConnection[contextParam->client_handle]->contextsCreated[contextParam->ctx_handle].vm = contextParam->param.value;
        break;
    case I915_CONTEXT_PARAM_ENGINES: {
        PRINT_DEBUGGER_INFO_LOG("I915_CONTEXT_PARAM_ENGINES ctx_id = %lu param = %llu value = %llu size = %lu",
                                (uint32_t)contextParam->param.ctx_id,
                                (uint64_t)contextParam->param.param,
                                (uint64_t)contextParam->param.value, (uint32_t)contextParam->param.size);

        auto numEngines = (contextParam->param.size - sizeof(i915_context_param_engines)) / sizeof(i915_engine_class_instance);
        auto engines = reinterpret_cast<i915_context_param_engines *>(&(contextParam->param.value));

        clientHandleToConnection[contextParam->client_handle]->contextsCreated[contextParam->ctx_handle].engines.clear();

        for (uint32_t i = 0; i < numEngines; i++) {
            clientHandleToConnection[contextParam->client_handle]->contextsCreated[contextParam->ctx_handle].engines.push_back(engines->engines[i]);
        }

        auto vm = clientHandleToConnection[contextParam->client_handle]->contextsCreated[contextParam->ctx_handle].vm;
        if (numEngines && vm != invalidHandle) {
            NEO::EngineClassInstance engineClassInstance = {engines->engines[0].engine_class, engines->engines[0].engine_instance};
            auto tileIndex = DrmHelper::getEngineTileIndex(connectedDevice, engineClassInstance);
            clientHandleToConnection[contextParam->client_handle]->vmToTile[vm] = tileIndex;

            PRINT_DEBUGGER_INFO_LOG("VM = %llu mapped to TILE = %lu\n", vm, tileIndex);
        }

        break;
    }
    default:
        PRINT_DEBUGGER_INFO_LOG("I915_CONTEXT_PARAM UNHANDLED = %llu\n", (uint64_t)contextParam->param.param);
        break;
    }
}

void DebugSessionLinux::handleAttentionEvent(prelim_drm_i915_debug_event_eu_attention *attention) {
    NEO::EngineClassInstance engineClassInstance = {attention->ci.engine_class, attention->ci.engine_instance};
    auto tileIndex = DrmHelper::getEngineTileIndex(connectedDevice, engineClassInstance);
    if (interruptSent && attention->base.seqno <= euControlInterruptSeqno[tileIndex]) {
        PRINT_DEBUGGER_INFO_LOG("Discarding EU ATTENTION event for interrupt request. Event seqno == %d <= %d == interrupt seqno\n",
                                (uint32_t)attention->base.seqno,
                                (uint32_t)euControlInterruptSeqno[tileIndex]);
        return;
    }

    newAttentionRaised(tileIndex);

    if (clientHandleToConnection.find(attention->client_handle) == clientHandleToConnection.end()) {
        return;
    }

    auto &clientConnection = clientHandleToConnection[attention->client_handle];
    if (clientConnection->lrcToContextHandle.find(attention->lrc_handle) == clientConnection->lrcToContextHandle.end()) {
        return;
    }

    auto contextHandle = clientConnection->lrcToContextHandle[attention->lrc_handle];
    if (clientConnection->contextsCreated.find(contextHandle) == clientConnection->contextsCreated.end()) {
        return;
    }

    auto vmHandle = clientConnection->contextsCreated[contextHandle].vm;
    if (vmHandle == invalidHandle) {
        return;
    }

    if (!connectedDevice->getNEODevice()->getDeviceBitfield().test(tileIndex)) {
        return;
    }

    auto hwInfo = connectedDevice->getHwInfo();
    auto &l0GfxCoreHelper = connectedDevice->getL0GfxCoreHelper();

    auto threadsWithAttention = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, tileIndex, attention->bitmask, attention->bitmask_size);

    printBitmask(attention->bitmask, attention->bitmask_size);

    PRINT_DEBUGGER_THREAD_LOG("ATTENTION for tile = %d thread count = %d\n", tileIndex, (int)threadsWithAttention.size());

    for (auto &threadId : threadsWithAttention) {
        PRINT_DEBUGGER_THREAD_LOG("ATTENTION event for thread: %s\n", EuThread::toString(threadId).c_str());

        if (tileSessionsEnabled) {
            static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(threadId, vmHandle);
        } else {
            markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(threadId, vmHandle);
        }
    }

    if (tileSessionsEnabled) {
        static_cast<TileDebugSessionLinux *>(tileSessions[tileIndex].first)->checkTriggerEventsForAttention();
    } else {
        checkTriggerEventsForAttention();
    }
}

void DebugSessionLinux::handleEnginesEvent(prelim_drm_i915_debug_event_engines *engines) {
    PRINT_DEBUGGER_INFO_LOG("ENGINES event: client_handle = %llu, ctx_handle = %llu, num_engines = %llu %s\n",
                            (uint64_t)engines->client_handle,
                            (uint64_t)engines->ctx_handle,
                            (uint64_t)engines->num_engines,
                            engines->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE ? "CREATE" : engines->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY ? "DESTROY"
                                                                                                                                                            : "");

    UNRECOVERABLE_IF(clientHandleToConnection.find(engines->client_handle) == clientHandleToConnection.end());

    if (engines->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE) {
        for (uint64_t i = 0; i < engines->num_engines; ++i) {
            auto lrc = engines->engines[i].lrc_handle;
            if (lrc != 0) {
                PRINT_DEBUGGER_INFO_LOG("    lrc%llu = %llu", i, lrc);
            }
            clientHandleToConnection[engines->client_handle]->lrcToContextHandle[lrc] = engines->ctx_handle;
        }
    }

    if (engines->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY) {
        for (uint64_t i = 0; i < engines->num_engines; ++i) {
            auto lrc = engines->engines[i].lrc_handle;
            PRINT_DEBUGGER_INFO_LOG("    lrc%llu = %llu\n", i, lrc);
            clientHandleToConnection[engines->client_handle]->lrcToContextHandle.erase(lrc);
        }
    }
}

void DebugSessionLinux::extractUuidData(uint64_t client, const UuidData &uuidData) {
    if (uuidData.classIndex == NEO::DrmResourceClass::SbaTrackingBuffer ||
        uuidData.classIndex == NEO::DrmResourceClass::ModuleHeapDebugArea ||
        uuidData.classIndex == NEO::DrmResourceClass::ContextSaveArea) {
        UNRECOVERABLE_IF(uuidData.dataSize != 8);
        uint64_t *data = (uint64_t *)uuidData.data.get();

        if (uuidData.classIndex == NEO::DrmResourceClass::SbaTrackingBuffer) {
            clientHandleToConnection[client]->stateBaseAreaGpuVa = *data;
            PRINT_DEBUGGER_INFO_LOG("SbaTrackingBuffer GPU VA = %p", (void *)clientHandleToConnection[clientHandle]->stateBaseAreaGpuVa);
        }
        if (uuidData.classIndex == NEO::DrmResourceClass::ModuleHeapDebugArea) {
            clientHandleToConnection[client]->moduleDebugAreaGpuVa = *data;
            PRINT_DEBUGGER_INFO_LOG("ModuleHeapDebugArea GPU VA = %p", (void *)clientHandleToConnection[clientHandle]->moduleDebugAreaGpuVa);
        }
        if (uuidData.classIndex == NEO::DrmResourceClass::ContextSaveArea) {
            clientHandleToConnection[client]->contextStateSaveAreaGpuVa = *data;
            PRINT_DEBUGGER_INFO_LOG("ContextSaveArea GPU VA = %p", (void *)clientHandleToConnection[clientHandle]->contextStateSaveAreaGpuVa);
        }
    }

    if (uuidData.classIndex == NEO::DrmResourceClass::L0ZebinModule) {
        uint32_t segmentCount = 0;
        memcpy_s(&segmentCount, sizeof(uint32_t), uuidData.data.get(), uuidData.dataSize);
        clientHandleToConnection[client]->uuidToModule[uuidData.handle].segmentCount = segmentCount;
    }
}

uint64_t DebugSessionLinux::extractVaFromUuidString(std::string &uuid) {
    const char uuidString[] = "%04" SCNx64 "-%012" SCNx64;
    auto subString = uuid.substr(19);

    uint64_t parts[2] = {0, 0};
    sscanf(subString.c_str(), uuidString, &parts[1], &parts[0]);

    parts[0] |= (parts[1] & 0xFFFF) << 48;
    return parts[0];
}

int DebugSessionLinux::threadControl(const std::vector<EuThread::ThreadId> &threads, uint32_t tile, ThreadControlCmd threadCmd, std::unique_ptr<uint8_t[]> &bitmaskOut, size_t &bitmaskSizeOut) {

    auto hwInfo = connectedDevice->getHwInfo();
    auto classInstance = DrmHelper::getEngineInstance(connectedDevice, tile, hwInfo.capabilityTable.defaultEngineType);
    UNRECOVERABLE_IF(!classInstance);

    auto &l0GfxCoreHelper = connectedDevice->getL0GfxCoreHelper();

    bitmaskSizeOut = 0;

    struct prelim_drm_i915_debug_eu_control euControl = {};
    euControl.client_handle = clientHandle;
    euControl.ci.engine_class = classInstance->engineClass;
    euControl.ci.engine_instance = classInstance->engineInstance;
    euControl.bitmask_size = 0;
    euControl.bitmask_ptr = 0;

    decltype(prelim_drm_i915_debug_eu_control::cmd) command = 0;
    switch (threadCmd) {
    case ThreadControlCmd::InterruptAll:
        command = PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT_ALL;
        break;
    case ThreadControlCmd::Interrupt:
        command = PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT;
        break;
    case ThreadControlCmd::Resume:
        command = PRELIM_I915_DEBUG_EU_THREADS_CMD_RESUME;
        break;
    case ThreadControlCmd::Stopped:
        command = PRELIM_I915_DEBUG_EU_THREADS_CMD_STOPPED;
        break;
    }
    euControl.cmd = command;

    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;

    if (command == PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT ||
        command == PRELIM_I915_DEBUG_EU_THREADS_CMD_RESUME ||
        command == PRELIM_I915_DEBUG_EU_THREADS_CMD_STOPPED) {
        l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, bitmaskSize);
        euControl.bitmask_size = static_cast<uint32_t>(bitmaskSize);
        euControl.bitmask_ptr = reinterpret_cast<uint64_t>(bitmask.get());
    }

    if (command == PRELIM_I915_DEBUG_EU_THREADS_CMD_RESUME) {
        applyResumeWa(bitmask.get(), bitmaskSize);
    }

    printBitmask(bitmask.get(), bitmaskSize);

    auto ret = ioctl(PRELIM_I915_DEBUG_IOCTL_EU_CONTROL, &euControl);
    if (ret != 0) {
        PRINT_DEBUGGER_ERROR_LOG("PRELIM_I915_DEBUG_IOCTL_EU_CONTROL failed: retCode: %d errno = %d command = %d\n", ret, errno, command);
    } else {
        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_EU_CONTROL: seqno = %llu command = %u\n", (uint64_t)euControl.seqno, command);
    }

    if (command == PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT ||
        command == PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT_ALL) {
        if (ret == 0) {
            euControlInterruptSeqno[tile] = euControl.seqno;
        } else {
            euControlInterruptSeqno[tile] = invalidHandle;
        }
    }

    if (threadCmd == ThreadControlCmd::Stopped) {
        bitmaskOut = std::move(bitmask);
        bitmaskSizeOut = euControl.bitmask_size;
    }
    return ret;
}

ze_result_t DebugSessionLinux::resumeImp(const std::vector<EuThread::ThreadId> &threads, uint32_t deviceIndex) {
    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize;

    auto result = threadControl(threads, deviceIndex, ThreadControlCmd::Resume, bitmask, bitmaskSize);

    return result == 0 ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_NOT_AVAILABLE;
}

ze_result_t DebugSessionLinux::interruptImp(uint32_t deviceIndex) {
    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize;

    auto result = threadControl({}, deviceIndex, ThreadControlCmd::InterruptAll, bitmask, bitmaskSize);

    return result == 0 ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_NOT_AVAILABLE;
}

ze_result_t DebugSessionLinux::getISAVMHandle(uint32_t deviceIndex, const zet_debug_memory_space_desc_t *desc, size_t size, uint64_t &vmHandle) {
    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
    auto accessVA = gmmHelper->decanonize(desc->address);
    auto &isaMap = clientHandleToConnection[clientHandle]->isaMap[deviceIndex];
    ze_result_t status = ZE_RESULT_ERROR_UNINITIALIZED;
    vmHandle = invalidHandle;

    if (isaMap.size() > 0) {
        uint64_t baseVa;
        uint64_t ceilVa;
        for (const auto &isa : isaMap) {
            baseVa = isa.second->bindInfo.gpuVa;
            ceilVa = isa.second->bindInfo.gpuVa + isa.second->bindInfo.size;
            if (accessVA >= baseVa && accessVA < ceilVa) {
                if (accessVA + size > ceilVa) {
                    status = ZE_RESULT_ERROR_INVALID_ARGUMENT;
                } else {
                    vmHandle = isa.second->vmHandle;
                    status = ZE_RESULT_SUCCESS;
                }
                break;
            }
        }
    }

    return status;
}

bool DebugSessionLinux::getIsaInfoForAllInstances(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, uint64_t vmHandles[], ze_result_t &status) {
    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
    auto accessVA = gmmHelper->decanonize(desc->address);

    status = ZE_RESULT_ERROR_UNINITIALIZED;

    bool tileInstancedIsa = false;
    bool invalidIsaRange = false;
    uint32_t isaFound = 0;

    for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
        vmHandles[i] = invalidHandle;

        if (deviceBitfield.test(i)) {

            auto &isaMap = clientHandleToConnection[clientHandle]->isaMap[i];
            if (isaMap.size() > 0) {
                uint64_t baseVa;
                uint64_t ceilVa;
                for (const auto &isa : isaMap) {
                    baseVa = isa.second->bindInfo.gpuVa;
                    ceilVa = isa.second->bindInfo.gpuVa + isa.second->bindInfo.size;
                    if (accessVA >= baseVa && accessVA < ceilVa) {
                        isaFound++;
                        if (accessVA + size > ceilVa) {
                            invalidIsaRange = true;
                        } else {
                            vmHandles[i] = isa.second->vmHandle;
                        }
                        tileInstancedIsa = isa.second->tileInstanced;
                        break;
                    }
                }
            }
        }
    }

    if (invalidIsaRange) {
        status = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    } else if (isaFound > 0) {
        if ((tileInstancedIsa && deviceBitfield.count() == isaFound) ||
            !tileInstancedIsa) {
            status = ZE_RESULT_SUCCESS;
        }
    }

    return isaFound > 0;
}

void DebugSessionLinux::printContextVms() {
    if (NEO::DebugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_INFO) {
        PRINT_DEBUGGER_LOG(stdout, "\nINFO: Context - VM map: ", "");
        for (size_t i = 0; i < clientHandleToConnection[clientHandle]->contextsCreated.size(); i++) {
            PRINT_DEBUGGER_LOG(stdout, "\n Context = %llu : %llu ", (uint64_t)clientHandleToConnection[clientHandle]->contextsCreated[i].handle,
                               (uint64_t)clientHandleToConnection[clientHandle]->contextsCreated[i].vm);
        }
    }
}

bool DebugSessionLinux::tryReadElf(const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, ze_result_t &status) {
    const char *elfData = nullptr;
    uint64_t offset = 0;

    std::lock_guard<std::mutex> memLock(asyncThreadMutex);

    status = getElfOffset(desc, size, elfData, offset);
    if (status == ZE_RESULT_ERROR_INVALID_ARGUMENT) {
        return true;
    }

    if (elfData) {
        status = readElfSpace(desc, size, buffer, elfData, offset);
        return true;
    }
    return false;
}

ze_result_t DebugSessionLinux::getElfOffset(const zet_debug_memory_space_desc_t *desc, size_t size, const char *&elfData, uint64_t &offset) {
    auto &elfMap = clientHandleToConnection[clientHandle]->elfMap;
    auto accessVA = desc->address;
    ze_result_t status = ZE_RESULT_ERROR_UNINITIALIZED;
    elfData = nullptr;

    if (elfMap.size() > 0) {
        uint64_t baseVa;
        uint64_t ceilVa;
        for (auto elf : elfMap) {
            baseVa = elf.first;
            ceilVa = elf.first + clientHandleToConnection[clientHandle]->uuidMap[elf.second].dataSize;
            if (accessVA >= baseVa && accessVA < ceilVa) {
                if (accessVA + size > ceilVa) {
                    status = ZE_RESULT_ERROR_INVALID_ARGUMENT;
                } else {
                    DEBUG_BREAK_IF(clientHandleToConnection[clientHandle]->uuidMap[elf.second].data.get() == nullptr);
                    elfData = clientHandleToConnection[clientHandle]->uuidMap[elf.second].data.get();
                    offset = accessVA - baseVa;
                    status = ZE_RESULT_SUCCESS;
                }
                break;
            }
        }
    }

    return status;
}

ze_result_t DebugSessionLinux::readElfSpace(const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer,
                                            const char *&elfData, const uint64_t offset) {

    int retVal = -1;
    elfData += offset;
    retVal = memcpy_s(buffer, size, elfData, size);
    return (retVal == 0) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t DebugSessionLinux::readMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) {
    ze_result_t status = validateThreadAndDescForMemoryAccess(thread, desc);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    if (desc->type == ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT) {
        status = readDefaultMemory(thread, desc, size, buffer);
    } else {
        auto threadId = convertToThreadId(thread);
        status = slmMemoryAccess<void *, false>(threadId, desc, size, buffer);
    }

    return status;
}

ze_result_t DebugSessionLinux::readDefaultMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) {
    ze_result_t status;

    bool isa = tryReadIsa(connectedDevice->getNEODevice()->getDeviceBitfield(), desc, size, buffer, status);
    if (isa) {
        return status;
    }

    bool elf = tryReadElf(desc, size, buffer, status);
    if (elf) {
        return status;
    }

    if (DebugSession::isThreadAll(thread)) {
        return accessDefaultMemForThreadAll(desc, size, const_cast<void *>(buffer), false);
    }

    auto threadId = convertToThreadId(thread);
    auto vmHandle = allThreads[threadId]->getMemoryHandle();
    if (vmHandle == invalidHandle) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    return readGpuMemory(vmHandle, static_cast<char *>(buffer), size, desc->address);
}

ze_result_t DebugSessionLinux::writeMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) {
    ze_result_t status = validateThreadAndDescForMemoryAccess(thread, desc);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    if (desc->type == ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT) {
        status = writeDefaultMemory(thread, desc, size, buffer);
    } else {
        auto threadId = convertToThreadId(thread);
        status = slmMemoryAccess<const void *, true>(threadId, desc, size, buffer);
    }

    return status;
}

ze_result_t DebugSessionLinux::writeDefaultMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) {
    ze_result_t status;

    auto deviceBitfield = connectedDevice->getNEODevice()->getDeviceBitfield();

    bool isa = tryWriteIsa(deviceBitfield, desc, size, buffer, status);
    if (isa) {
        return status;
    }

    if (DebugSession::isThreadAll(thread)) {
        return accessDefaultMemForThreadAll(desc, size, const_cast<void *>(buffer), true);
    }

    auto threadId = convertToThreadId(thread);
    auto threadVmHandle = allThreads[threadId]->getMemoryHandle();
    if (threadVmHandle == invalidHandle) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    return writeGpuMemory(threadVmHandle, static_cast<const char *>(buffer), size, desc->address);
}

bool DebugSessionLinux::tryWriteIsa(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer, ze_result_t &status) {
    return tryAccessIsa(deviceBitfield, desc, size, const_cast<void *>(buffer), true, status);
}

bool DebugSessionLinux::tryReadIsa(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, ze_result_t &status) {
    return tryAccessIsa(deviceBitfield, desc, size, buffer, false, status);
}

bool DebugSessionLinux::tryAccessIsa(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, bool write, ze_result_t &status) {
    status = ZE_RESULT_ERROR_NOT_AVAILABLE;
    uint64_t vmHandle[NEO::EngineLimits::maxHandleCount] = {invalidHandle};
    uint32_t deviceIndex = Math::getMinLsbSet(static_cast<uint32_t>(deviceBitfield.to_ulong()));

    bool isaAccess = false;

    auto checkIfAnyFailed = [](const auto &result) { return result != ZE_RESULT_SUCCESS; };

    {
        std::lock_guard<std::mutex> memLock(asyncThreadMutex);

        if (deviceBitfield.count() == 1) {
            status = getISAVMHandle(deviceIndex, desc, size, vmHandle[deviceIndex]);
            if (status == ZE_RESULT_SUCCESS) {
                isaAccess = true;
            }
            if (status == ZE_RESULT_ERROR_INVALID_ARGUMENT) {
                return true;
            }
        } else {
            isaAccess = getIsaInfoForAllInstances(deviceBitfield, desc, size, vmHandle, status);
        }
    }

    if (isaAccess && status == ZE_RESULT_SUCCESS) {

        if (write) {
            if (deviceBitfield.count() == 1) {
                if (vmHandle[deviceIndex] != invalidHandle) {
                    status = writeGpuMemory(vmHandle[deviceIndex], static_cast<char *>(buffer), size, desc->address);
                } else {
                    status = ZE_RESULT_ERROR_UNINITIALIZED;
                }
            } else {
                std::vector<ze_result_t> results(NEO::EngineLimits::maxHandleCount);

                for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
                    results[i] = ZE_RESULT_SUCCESS;

                    if (deviceBitfield.test(i) && vmHandle[i] != invalidHandle) {
                        results[i] = writeGpuMemory(vmHandle[i], static_cast<char *>(buffer), size, desc->address);

                        if (results[i] != ZE_RESULT_SUCCESS) {
                            break;
                        }
                    }
                }

                const bool anyFailed = std::any_of(results.begin(), results.end(), checkIfAnyFailed);

                if (anyFailed) {
                    status = ZE_RESULT_ERROR_UNKNOWN;
                }
            }
        } else {

            if (deviceBitfield.count() > 1) {
                for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
                    if (vmHandle[i] != invalidHandle) {
                        deviceIndex = i;
                        break;
                    }
                }
            }

            if (vmHandle[deviceIndex] != invalidHandle) {
                status = readGpuMemory(vmHandle[deviceIndex], static_cast<char *>(buffer), size, desc->address);
            } else {
                status = ZE_RESULT_ERROR_UNINITIALIZED;
            }
        }
    }
    return isaAccess;
}

ze_result_t DebugSessionLinux::accessDefaultMemForThreadAll(const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, bool write) {
    auto status = ZE_RESULT_ERROR_UNINITIALIZED;
    std::vector<uint64_t> allVms;

    allVms = getAllMemoryHandles();

    if (allVms.size() > 0) {
        for (auto vmHandle : allVms) {
            if (write) {
                status = writeGpuMemory(vmHandle, static_cast<char *>(buffer), size, desc->address);
            } else {
                status = readGpuMemory(vmHandle, static_cast<char *>(buffer), size, desc->address);
            }
            if (status == ZE_RESULT_SUCCESS) {
                return status;
            }
        }

        status = ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    return status;
}

bool DebugSessionLinux::ackIsaEvents(uint32_t deviceIndex, uint64_t isaVa) {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);

    auto connection = clientHandleToConnection[clientHandle].get();

    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
    auto isaVaStart = gmmHelper->decanonize(isaVa);
    auto isa = connection->isaMap[deviceIndex].find(isaVaStart);

    if (isa != connection->isaMap[deviceIndex].end()) {

        // zebin modules do not store ackEvents per ISA
        UNRECOVERABLE_IF(isa->second->ackEvents.size() > 0 && isa->second->perKernelModule == false);

        for (auto &event : isa->second->ackEvents) {
            prelim_drm_i915_debug_event_ack eventToAck = {};
            eventToAck.type = event.type;
            eventToAck.seqno = event.seqno;
            eventToAck.flags = 0;

            auto ret = ioctl(PRELIM_I915_DEBUG_IOCTL_ACK_EVENT, &eventToAck);
            PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_ACK_EVENT seqno = %llu, ret = %d errno = %d\n", (uint64_t)event.seqno, ret, ret != 0 ? errno : 0);
        }

        isa->second->ackEvents.clear();
        isa->second->moduleLoadEventAck = true;
        return true;
    }
    return false;
}

bool DebugSessionLinux::ackModuleEvents(uint32_t deviceIndex, uint64_t moduleUuidHandle) {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);

    auto connection = clientHandleToConnection[clientHandle].get();

    if (connection->uuidToModule.find(moduleUuidHandle) != connection->uuidToModule.end()) {
        auto &module = connection->uuidToModule[moduleUuidHandle];
        for (auto &event : module.ackEvents[deviceIndex]) {

            prelim_drm_i915_debug_event_ack eventToAck = {};
            eventToAck.type = event.type;
            eventToAck.seqno = event.seqno;
            eventToAck.flags = 0;

            auto ret = ioctl(PRELIM_I915_DEBUG_IOCTL_ACK_EVENT, &eventToAck);
            PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_ACK_EVENT seqno = %llu, ret = %d errno = %d\n", (uint64_t)event.seqno, ret, ret != 0 ? errno : 0);
        }
        module.ackEvents[deviceIndex].clear();
        module.moduleLoadEventAcked[deviceIndex] = true;

        return true;
    }
    DEBUG_BREAK_IF(true);
    return false;
}

void DebugSessionLinux::cleanRootSessionAfterDetach(uint32_t deviceIndex) {
    auto connection = clientHandleToConnection[clientHandle].get();

    for (const auto &isa : connection->isaMap[deviceIndex]) {

        // zebin modules do not store ackEvents per ISA
        UNRECOVERABLE_IF(isa.second->ackEvents.size() > 0 && isa.second->perKernelModule == false);

        for (auto &event : isa.second->ackEvents) {
            prelim_drm_i915_debug_event_ack eventToAck = {};
            eventToAck.type = event.type;
            eventToAck.seqno = event.seqno;
            eventToAck.flags = 0;

            auto ret = ioctl(PRELIM_I915_DEBUG_IOCTL_ACK_EVENT, &eventToAck);
            PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_ACK_EVENT seqno = %llu, ret = %d errno = %d\n", (uint64_t)event.seqno, ret, ret != 0 ? errno : 0);
        }

        isa.second->ackEvents.clear();
        isa.second->moduleLoadEventAck = true;
    }
}

ze_result_t DebugSessionLinux::acknowledgeEvent(const zet_debug_event_t *event) {

    const zet_debug_event_t apiEventToAck = *event;
    {
        std::unique_lock<std::mutex> lock(asyncThreadMutex);

        for (size_t i = 0; i < eventsToAck.size(); i++) {
            if (apiEventCompare(apiEventToAck, eventsToAck[i].first)) {

                auto moduleUUID = eventsToAck[i].second;
                auto iter = eventsToAck.begin() + i;
                eventsToAck.erase(iter);

                lock.unlock();

                for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
                    if (connectedDevice->getNEODevice()->getDeviceBitfield().test(i)) {
                        ackModuleEvents(i, moduleUUID);
                    }
                }

                return ZE_RESULT_SUCCESS;
            }
        }
    }

    if (apiEventToAck.type == ZET_DEBUG_EVENT_TYPE_MODULE_LOAD) {
        bool allIsaAcked = true;
        for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
            if (connectedDevice->getNEODevice()->getDeviceBitfield().test(i)) {
                if (!ackIsaEvents(i, apiEventToAck.info.module.load)) {
                    allIsaAcked = false;
                }
            }
        }
        if (allIsaAcked) {
            return ZE_RESULT_SUCCESS;
        }
    }

    return ZE_RESULT_ERROR_UNINITIALIZED;
}

ze_result_t DebugSessionLinux::readSbaBuffer(EuThread::ThreadId threadId, NEO::SbaTrackedAddresses &sbaBuffer) {
    auto vmHandle = allThreads[threadId]->getMemoryHandle();

    if (vmHandle == invalidHandle) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    auto gpuVa = getSbaBufferGpuVa(vmHandle);
    if (gpuVa == 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return readGpuMemory(vmHandle, reinterpret_cast<char *>(&sbaBuffer), sizeof(sbaBuffer), gpuVa);
}

uint64_t DebugSessionLinux::getSbaBufferGpuVa(uint64_t memoryHandle) {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    auto bindInfo = clientHandleToConnection[clientHandle]->vmToStateBaseAreaBindInfo.find(memoryHandle);
    if (bindInfo == clientHandleToConnection[clientHandle]->vmToStateBaseAreaBindInfo.end()) {
        return 0;
    }

    return bindInfo->second.gpuVa;
}

uint64_t DebugSessionLinux::getContextStateSaveAreaGpuVa(uint64_t memoryHandle) {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    auto bindInfo = clientHandleToConnection[clientHandle]->vmToContextStateSaveAreaBindInfo.find(memoryHandle);
    if (bindInfo == clientHandleToConnection[clientHandle]->vmToContextStateSaveAreaBindInfo.end()) {
        return 0;
    }

    return bindInfo->second.gpuVa;
}

void TileDebugSessionLinux::readStateSaveAreaHeader() {

    const auto header = rootDebugSession->getStateSaveAreaHeader();
    if (header) {
        auto headerSize = rootDebugSession->stateSaveAreaHeader.size();
        this->stateSaveAreaHeader.assign(reinterpret_cast<const char *>(header), reinterpret_cast<const char *>(header) + headerSize);
    }
};

bool TileDebugSessionLinux::insertModule(zet_debug_event_info_module_t module) {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    modules.insert({module.load, module});
    return isAttached;
}

bool TileDebugSessionLinux::removeModule(zet_debug_event_info_module_t module) {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    modules.erase(module.load);
    return isAttached;
}

bool TileDebugSessionLinux::processEntry() {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    processEntryState = true;
    return isAttached;
}

bool TileDebugSessionLinux::processExit() {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    processEntryState = false;
    return isAttached;
}

void TileDebugSessionLinux::attachTile() {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);

    // clear apiEvents queue
    apiEvents = decltype(apiEvents){};

    if (detached) {
        zet_debug_event_t debugEvent = {};
        debugEvent.type = ZET_DEBUG_EVENT_TYPE_DETACHED;
        debugEvent.info.detached.reason = ZET_DEBUG_DETACH_REASON_INVALID;

        apiEvents.push(debugEvent);
    } else {
        if (processEntryState) {
            zet_debug_event_t event = {};
            event.type = ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY;
            event.flags = 0;
            apiEvents.push(event);
        }

        if (modules.size()) {
            zet_debug_event_t event = {};
            event.type = ZET_DEBUG_EVENT_TYPE_MODULE_LOAD;
            event.flags = 0;

            for (const auto &module : modules) {
                memcpy_s(&event.info.module, sizeof(event.info.module), &module.second, sizeof(module.second));
                apiEvents.push(event);
            }
        }
    }
    isAttached = true;
}

void TileDebugSessionLinux::detachTile() {
    std::vector<uint64_t> moduleUuids;
    {
        std::lock_guard<std::mutex> lock(asyncThreadMutex);
        for (const auto &eventToAck : eventsToAck) {
            auto moduleUUID = eventToAck.second;
            moduleUuids.push_back(moduleUUID);
        }
        eventsToAck.clear();
        isAttached = false;
    }

    for (const auto &uuid : moduleUuids) {
        rootDebugSession->ackModuleEvents(this->tileIndex, uuid);
    }
}

} // namespace L0
