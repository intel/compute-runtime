/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"
#include "level_zero/tools/source/debug/linux/debug_session.h"
#include "level_zero/tools/source/debug/linux/debug_session_factory.h"

#include "uapi-eudebug/drm/xe_drm.h"
#include "uapi-eudebug/drm/xe_drm_tmp.h"

namespace L0 {

struct DebugSessionLinuxXe : DebugSessionLinux {

    ~DebugSessionLinuxXe() override;
    DebugSessionLinuxXe(const zet_debug_config_t &config, Device *device, int debugFd, void *params);
    static DebugSession *createLinuxSession(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach);

    ze_result_t initialize() override;

    bool closeConnection() override;

    ze_result_t readMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t writeMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t acknowledgeEvent(const zet_debug_event_t *event) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    void startAsyncThread() override {
    }

    bool readModuleDebugArea() override {
        UNRECOVERABLE_IF(true);
        return true;
    }

    ze_result_t readSbaBuffer(EuThread::ThreadId threadId, NEO::SbaTrackedAddresses &sbaBuffer) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    void cleanRootSessionAfterDetach(uint32_t deviceIndex) override {
        UNRECOVERABLE_IF(true);
    }

    ze_result_t resumeImp(const std::vector<EuThread::ThreadId> &threads, uint32_t deviceIndex) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t interruptImp(uint32_t deviceIndex) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t readGpuMemory(uint64_t memoryHandle, char *output, size_t size, uint64_t gpuVa) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t writeGpuMemory(uint64_t memoryHandle, const char *input, size_t size, uint64_t gpuVa) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    void enqueueApiEvent(zet_debug_event_t &debugEvent) override {
        UNRECOVERABLE_IF(true);
    }

    uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override {
        UNRECOVERABLE_IF(true);
        return 0;
    }

    size_t getContextStateSaveAreaSize(uint64_t memoryHandle) override {
        UNRECOVERABLE_IF(true);
        return 0;
    }

    void attachTile() override {
        UNRECOVERABLE_IF(true);
    }
    void detachTile() override {
        UNRECOVERABLE_IF(true);
    }

    struct IoctlHandlerXe : DebugSessionLinux::IoctlHandler {
        int ioctl(int fd, unsigned long request, void *arg) override {
            int ret = 0;
            int error = 0;
            bool shouldRetryIoctl = false;
            do {
                shouldRetryIoctl = false;
                ret = NEO::SysCalls::ioctl(fd, request, arg);
                error = errno;

                if (ret == -1) {
                    shouldRetryIoctl = (error == EINTR || error == EAGAIN || error == EBUSY);

                    if (request == DRM_XE_EUDEBUG_IOCTL_EU_CONTROL) {
                        shouldRetryIoctl = (error == EINTR || error == EAGAIN);
                    }
                }
            } while (shouldRetryIoctl);
            return ret;
        }
    };

    using ContextHandle = uint64_t;

    struct ContextParams {
        ContextHandle handle = 0;
        uint64_t vm = UINT64_MAX;
        std::vector<drm_xe_engine_class_instance> engines;
    };

    struct BindInfo {
        uint64_t gpuVa = 0;
        uint64_t size = 0;
    };

    struct ClientConnection {
        drm_xe_eudebug_event_client client = {};
        std::unordered_map<uint64_t, BindInfo> vmToModuleDebugAreaBindInfo;
    };

    bool checkAllEventsCollected();
    void handleEvent(drm_xe_eudebug_event *event);
    void readInternalEventsAsync() override;
    void pushApiEvent(zet_debug_event_t &debugEvent);
    std::unique_ptr<IoctlHandlerXe> ioctlHandler;
    uint32_t xeDebuggerVersion = 0;
    std::unordered_map<uint64_t, std::unique_ptr<ClientConnection>> clientHandleToConnection;
    std::atomic<bool> detached{false};

    ze_result_t readEventImp(drm_xe_eudebug_event *drmDebugEvent);
    int ioctl(unsigned long request, void *arg);
};

} // namespace L0
